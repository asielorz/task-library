#include "profiler.hh"
#include <cassert>
#include <fstream>
#include <array>
#include <map>

struct NodeInDisk
{
	StringInDisk name;
	std::chrono::nanoseconds time_start;
	std::chrono::nanoseconds time_end;
	uint16_t parent;
	uint16_t first_child;
	uint16_t next_sibling;
};

struct ProfilesFileHeader
{
	static constexpr std::array<char, 8> correct_header_identifier = {'P', 'R', 'O', 'F', 'I', 'L', 'E', 'R'}; 
	std::array<char, 8> header_identifier;
	uint32_t profile_count;
};

struct ProfileInDiskHeader
{
	StringInDisk parent_id;
	uint32_t node_count;
};

StringInDisk record_string(std::vector<char> strings, std::string_view str, std::map<std::string_view, StringInDisk> & already_recorded_strings)
{
	if (str.empty())
		return {0, 0};

	if (auto const it = already_recorded_strings.find(str); it != already_recorded_strings.end())
		return it->second;

	StringInDisk string_in_disk;
	string_in_disk.start = static_cast<uint32_t>(strings.size());
	string_in_disk.length = static_cast<uint32_t>(str.length());

	for (char c : str) strings.push_back(c);
	already_recorded_strings[str] = string_in_disk;

	return string_in_disk;
}

std::string_view resolve_string(std::span<char const> strings, StringInDisk str)
{
	if (str.length == 0)
		return "";

	else
		return {strings.data() + str.start, str.length};
}

void save_profiles(
	std::span<TaskProfile const> profiles, std::ostream & out, 
	std::vector<char> & strings, 
	std::map<std::string_view, StringInDisk> & already_recorded_strings
)
{
	std::vector<NodeInDisk> nodes_in_disk;
	
	ProfilesFileHeader const file_header = {
		.header_identifier = ProfilesFileHeader::correct_header_identifier,
		.profile_count = static_cast<uint32_t>(profiles.size()),
	};
	out.write(reinterpret_cast<char const *>(&file_header), sizeof(ProfilesFileHeader));

	for (TaskProfile const & profile : profiles)
	{
		nodes_in_disk.clear();
		nodes_in_disk.reserve(profile.nodes.size());

		ProfileInDiskHeader const header = {
			.parent_id = record_string(strings, profile.parent_id, already_recorded_strings),
			.node_count = static_cast<uint32_t>(profile.nodes.size()),
		};

		for (TaskProfile::Node const & node : profile.nodes)
		{
			NodeInDisk const node_in_disk = {
				.name			= record_string(strings, node.name, already_recorded_strings),
				.time_start		= node.time_start,
				.time_end		= node.time_end,
				.parent			= node.parent,
				.first_child	= node.first_child,
				.next_sibling	= node.next_sibling,
			};
			nodes_in_disk.push_back(node_in_disk);
		}

		out.write(reinterpret_cast<char const *>(&header), sizeof(ProfileInDiskHeader));
		out.write(reinterpret_cast<char const *>(nodes_in_disk.data()), nodes_in_disk.size() * sizeof(NodeInDisk));
	}
}

std::vector<TaskProfile> load_profiles(std::istream & in, std::span<char const> strings)
{
	std::vector<TaskProfile> profiles;
	std::vector<NodeInDisk> nodes_in_disk;

	while (!in.eof())
	{
		ProfilesFileHeader file_header;
		in.read(reinterpret_cast<char *>(&file_header), sizeof(ProfilesFileHeader));

		if (file_header.header_identifier != ProfilesFileHeader::correct_header_identifier)
			break;

		for (uint32_t profile_i = 0; profile_i < file_header.profile_count; ++profile_i)
		{
			ProfileInDiskHeader header;
			in.read(reinterpret_cast<char *>(&header), sizeof(ProfileInDiskHeader));

			TaskProfile profile;
			profile.parent_id = resolve_string(strings, header.parent_id);
			profile.nodes.reserve(header.node_count);

			nodes_in_disk.resize(header.node_count);
			in.read(reinterpret_cast<char *>(nodes_in_disk.data()), nodes_in_disk.size() * sizeof(NodeInDisk));

			for (NodeInDisk const & node_in_disk : nodes_in_disk)
			{
				profile.nodes.push_back({
					.name			= resolve_string(strings, node_in_disk.name),
					.time_start		= node_in_disk.time_start,
					.time_end		= node_in_disk.time_end,
					.parent			= node_in_disk.parent,
					.first_child	= node_in_disk.first_child,
					.next_sibling	= node_in_disk.next_sibling,
				});
			}

			profiles.push_back(std::move(profile));
		}
	}

	return profiles;
}

uint16_t push_node(std::vector<TaskProfile::Node> & nodes, std::string_view name, uint16_t parent)
{
	auto index = static_cast<uint16_t>(nodes.size());
	nodes.push_back(TaskProfile::Node{
		.name = name,
		.time_start = std::chrono::steady_clock::now().time_since_epoch(),
		.parent = parent
	});
	return index;
}

void add_child(std::vector<TaskProfile::Node> & nodes, uint16_t parent, uint16_t child)
{
	TaskProfile::Node & parent_node = nodes[parent];
	if (parent_node.first_child == TaskProfile::invalid_node_index)
	{
		parent_node.first_child = child;
		return;
	}

	TaskProfile::Node * last_child = &nodes[parent_node.first_child];
	while (last_child->next_sibling != TaskProfile::invalid_node_index)
		last_child = &nodes[last_child->next_sibling];

	last_child->next_sibling = child;
}

void Profiler::start_main_task(std::string_view name)
{
	assert(current_node == TaskProfile::invalid_node_index);
	start_sub_task(name, TaskProfile::no_parent_id);
}

void Profiler::start_sub_task(std::string_view name, std::string_view parent_id)
{
	assert(current_node == TaskProfile::invalid_node_index);
	current_profile.parent_id = parent_id;
	current_profile.nodes.reserve(64);
	current_node = push_node(current_profile.nodes, name, TaskProfile::invalid_node_index);
}

void Profiler::end_task()
{
	pop();
	assert(current_node == TaskProfile::invalid_node_index);
	auto const g = std::lock_guard(finished_profiles_mutex);
	finished_profiles.push_back(std::move(current_profile));
}

void Profiler::push(std::string_view name)
{
	auto const new_node_index = push_node(current_profile.nodes, name, current_node);
	add_child(current_profile.nodes, current_node, new_node_index);
	current_node = new_node_index;
}

void Profiler::pop() noexcept
{
	current_profile.nodes[current_node].time_end = std::chrono::steady_clock::now().time_since_epoch();
	current_node = current_profile.nodes[current_node].parent;
}

std::vector<TaskProfile> Profiler::get_finished_profiles() noexcept
{
	auto const g = std::lock_guard(finished_profiles_mutex);
	return std::move(finished_profiles);
}
