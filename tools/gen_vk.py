#!/usr/bin/env python3

import operator
import sys
from argparse import ArgumentParser
from operator import itemgetter
from urllib import request
from xml.etree import ElementTree

from ordered_set import OrderedSet


# Rewrite a vulkan type.
def convert_type(orig):
    if orig == 'VkBool32':
        return 'Bool'
    if 'Flags' in orig or 'FlagBits' in orig:
        new_suffix = ''
        if 'Flags' in orig:
            index = orig.index('Flags')
            if index + 5 < len(orig):
                new_suffix += orig[index + 5:]
            orig = orig[:index]
        if 'FlagBits' in orig:
            index = orig.index('FlagBits')
            if index + 8 < len(orig):
                new_suffix += orig[index + 8:]
            orig = orig[:index]

        # Hardcoded list of enum endings to omit the Flags suffix from.
        endings = ['Access', 'Aspect', 'Component', 'Count', 'Feature', 'Mode', 'Stage', 'Usage']
        if not any([orig.endswith(ending) for ending in endings]):
            orig += 'Flags'
        orig += new_suffix

    # Strip Vk prefix, we have our own namespace.
    return orig[2:] if orig.startswith('Vk') else orig


# A post order depth first search to visit the type order graph.
def dfs(graph, visited, node, action):
    if node not in visited and node in graph:
        visited.add(node)
        for successor in graph[node]:
            dfs(graph, visited, successor, action)
        action(node)


# https://www.khronos.org/registry/vulkan/specs/1.3/styleguide.html#_assigning_extension_token_values
def enum_offset(ext_num, offset):
    return 1000000000 + (ext_num - 1) * 1000 + offset


# Returns true if parent_name is a parent type of derived_name (e.g. VkDevice is a parent of VkCommandBuffer).
def is_parent_of(parent_name, derived_name):
    if parent_name == derived_name:
        return True
    derived = type_dict.get(derived_name)
    if not derived:
        return False
    parents = derived.get('parent')
    if not parents:
        return False
    return any([is_parent_of(parent_name, p) for p in parents.split(',')])


def evaluate_depends(attribute, contains_set):
    token = ''
    tokens = []
    for char in attribute:
        if char not in '+,()':
            token += char
        else:
            if token:
                tokens.append(token)
            tokens.append(char)
            token = ''
    if token:
        tokens.append(token)

    operator_stack = []
    rpn = []
    for token in tokens:
        if token not in '+,()':
            rpn.append(token)
        elif token in '+,':
            while operator_stack and operator_stack[-1] in '+,':
                rpn.append(operator_stack.pop())
            operator_stack.append(token)
        elif token == '(':
            operator_stack.append('(')
        elif token == ')':
            while operator_stack[-1] != '(':
                rpn.append(operator_stack.pop())
            operator_stack.pop()
    while operator_stack:
        rpn.append(operator_stack.pop())

    stack = []
    for atom in rpn:
        if atom not in '+,':
            stack.append(atom in contains_set)
        else:
            op_fns = {'+': operator.and_, ',': operator.or_}
            rhs = stack.pop()
            lhs = stack.pop()
            stack.append(op_fns[atom](lhs, rhs))

    assert len(stack) == 1
    return stack.pop()


def parse_depends(element):
    if attribute := element.get('depends'):
        assert all(char not in attribute for char in ['(', ')', ','])
        return list(attribute.split('+'))
    return []


args_parser = ArgumentParser(description='Parse vk.xml')
args_parser.add_argument('--download-latest', action='store_true')
args = args_parser.parse_args()
if args.download_latest:
    request.urlretrieve('https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml',
                        'vk.xml')
    sys.exit(0)

with open('vk.xml', 'rb') as file:
    registry = ElementTree.parse(file)

# OrderedSet to ensure generation is deterministic.
desired_extension_names = OrderedSet([
    'VK_EXT_debug_utils',
    'VK_EXT_descriptor_buffer',
    'VK_EXT_shader_atomic_float2',
    'VK_EXT_validation_features',
    'VK_KHR_surface',
    'VK_KHR_swapchain',
    'VK_KHR_xcb_surface',
])

# Make sure any dependency extensions are added.
for extension_name in desired_extension_names.copy():
    extension = registry.find('.//extension[@name="{}"]'.format(extension_name))
    assert extension
    for dependency in filter(lambda x: x not in desired_extension_names, parse_depends(extension)):
        print('Implicitly generating {}, needed by {}'.format(dependency, extension_name))
        desired_extension_names.add(dependency)

# Build a dictionary of command names (e.g. 'vkCreateImage') to command elements, first handling the base commands.
command_dict = {}
for command in filter(lambda cmd: not cmd.get('alias'), registry.findall('commands/command')):
    command_dict[command.findtext('proto/name')] = command

# Handle any aliases, but effectively discard them by making them point to the original type as we don't want to
# generate aliases in our header.
for alias in filter(lambda cmd: cmd.get('alias'), registry.findall('commands/command')):
    command_dict[alias.get('name')] = command_dict[alias.get('alias')]

# Build a dictionary of type names (e.g. 'VkImage') to type elements.
type_dict = {}
for vk_type in registry.findall('types/type'):
    name = vk_type.findtext('name') or vk_type.get('name')
    assert name
    type_dict[name] = vk_type

# Build a list of XML paths from enabled core features and extensions.
enabled_feature_names = []
enabled_feature_paths = []
for core_version in ['1_0', '1_1', '1_2', '1_3']:
    enabled_feature_names.append('VK_VERSION_{}'.format(core_version))
    enabled_feature_paths.append('.//feature[@name="VK_VERSION_{}"]'.format(core_version))
for extension_name in desired_extension_names:
    enabled_feature_names.append(extension_name)
    enabled_feature_paths.append('.//extension[@name="{}"]'.format(extension_name))

# Build a list of desired commands, enum extensions and types.
desired_command_names = []
desired_enum_extensions = []
desired_type_names = []
for feature in map(lambda path: registry.find(path), enabled_feature_paths):
    assert feature and feature.get('supported') != 'disabled'

    # Presence of type attribute signifies whether this feature is a core feature or an extension.
    extension_number = feature.get('number') if feature.get('type') else None

    feature_name = feature.get('name')
    for functionality in feature.findall('require'):
        if depends := functionality.get('depends'):
            if not evaluate_depends(depends, enabled_feature_names):
                print('Skipping functionality of {} gated behind {}'.format(feature_name, depends))
                continue
        for command in functionality.findall('command'):
            desired_command_names.append(command.get('name'))
        for enum_extension in functionality.findall('enum'):
            desired_enum_extensions.append((enum_extension, extension_number))
        for vk_type in functionality.findall('type'):
            desired_type_names.append(vk_type.get('name'))

# Build a list of desired commands to be emitted.
desired_commands = []
for command_name in desired_command_names:
    desired_commands.append(command_dict.get(command_name))

# Alphabetically sort and deduplicate commands.
desired_commands.sort(key=lambda cmd: cmd.findtext('proto/name'))
desired_commands = list(dict.fromkeys(desired_commands))

for enum_extension, extnumber in filter(lambda e: e[0].get('extends') and not e[0].get('alias'),
                                        desired_enum_extensions):
    definition = registry.find('.//enums[@name="{}"]'.format(enum_extension.get('extends')))
    direction = enum_extension.get('dir') or '+'
    extnumber = enum_extension.get('extnumber') or extnumber

    value = None
    if not enum_extension.get('bitpos'):
        value = enum_extension.get('value') or enum_offset(int(extnumber), int(enum_extension.get('offset')))
    if direction == '-':
        value = -value

    assert not enum_extension.get('type')
    definition.append(definition.makeelement('enum', {
        'bitpos': enum_extension.get('bitpos'),
        'name': enum_extension.get('name'),
        'value': value,
    }))

# Build a type order graph. We need this for emitting types in the right order.
type_order_graph = {}
for type_name in desired_type_names:
    vk_type = type_dict.get(type_name)
    if type_name not in type_order_graph:
        type_order_graph[type_name] = OrderedSet()

    for param_type in vk_type.findall('type'):
        if param_type.text in desired_type_names:
            type_order_graph[type_name].add(param_type.text)

    for member in vk_type.findall('member'):
        if member_type := member.findtext('type'):
            type_order_graph[type_name].add(member_type)

            # Some types (e.g. VkSurfaceTransformFlagsKHR) aren't included as a type in the extension definition.
            if member_type not in desired_type_names:
                desired_type_names.append(member_type)

# Build an ordered list of desired types to be emitted.
desired_types = []
type_order_visited = set()
for type_name in desired_type_names:
    vk_type = type_dict.get(type_name)

    # Type order graph leaf nodes become DFS root nodes.
    has_users = False
    for use_set in type_order_graph.values():
        if type_name in use_set:
            has_users = True
            break

    if has_users:
        continue

    dfs(type_order_graph, type_order_visited, type_name, lambda node: desired_types.append((node, type_dict.get(node))))

# Generate context table header.
with open('../engine/include/vull/vulkan/ContextTable.hh', 'w') as file:
    file.write('''// File generated by tools/gen_vk.py
// NOLINTBEGIN
#pragma once

#include <vull/vulkan/Vulkan.hh>

namespace vull::vkb {

class ContextTable {
protected:
    Instance m_instance;
    PhysicalDevice m_physical_device;
    Device m_device;

    void load_loader(PFN_vkGetInstanceProcAddr get_instance_proc_addr);
    void load_instance(PFN_vkGetInstanceProcAddr get_instance_proc_addr);
    void load_device();

private:
''')
    for command in desired_commands:
        name = command.findtext('proto/name')
        if name == 'vkGetInstanceProcAddr':
            continue
        file.write('    PFN_' + name + ' m_' + name + ';\n')
    file.write('\npublic:\n')
    for command in desired_commands:
        name = command.findtext('proto/name')
        if name == 'vkGetInstanceProcAddr':
            continue

        # Write function prototype.
        file.write('    {} {}('.format(convert_type(command.findtext('proto/type')), name))
        param_index = 0
        for param in command.findall('param'):
            if api := param.get('api'):
                if api == 'vulkansc':
                    continue
            param_name = param.findtext('name')
            if param_name == 'device' or param_name == 'instance' or param_name == 'physicalDevice' \
                    or param_name == 'pAllocator':
                continue
            if param_index != 0:
                file.write(', ')
            param_str = ''
            for param_part in param.itertext():
                if len(param_part.strip()) != 0:
                    param_str += convert_type(param_part.strip())
                    if len(param_part.strip()) != 1:
                        param_str += ' '
            file.write(param_str.strip())
            param_index += 1
        file.write(') const;\n')

    file.write('};\n\n')
    file.write('} // namespace vull::vkb\n')
    file.write('// NOLINTEND\n')

# Generate context table source file.
with open('../engine/sources/vulkan/ContextTable.cc', 'w') as file:
    file.write('''// File generated by tools/gen_vk.py
// NOLINTBEGIN
#include <vull/vulkan/ContextTable.hh>

namespace vull::vkb {

''')

    loader_commands = []
    instance_commands = []
    device_commands = []
    for command in desired_commands:
        name = command.findtext('proto/name')
        if name == 'vkGetInstanceProcAddr':
            continue

        vk_type = command.findtext('param[1]/type')
        if name == 'vkGetDeviceProcAddr':
            vk_type = 'VkInstance'

        if is_parent_of('VkDevice', vk_type):
            device_commands.append(name)
        elif is_parent_of('VkInstance', vk_type):
            instance_commands.append(name)
        else:
            loader_commands.append(name)

    file.write('void ContextTable::load_loader(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr) {\n')
    for command in loader_commands:
        file.write('    m_{0} = reinterpret_cast<PFN_{0}>(vkGetInstanceProcAddr(nullptr, "{0}"));\n'.format(command))
    file.write('}\n\n')

    file.write('void ContextTable::load_instance(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr) {\n')
    for command in instance_commands:
        file.write('    m_{0} = reinterpret_cast<PFN_{0}>(vkGetInstanceProcAddr(m_instance, "{0}"));\n'.format(command))
    file.write('}\n\n')

    file.write('void ContextTable::load_device() {\n')
    for command in device_commands:
        file.write('    m_{0} = reinterpret_cast<PFN_{0}>(vkGetDeviceProcAddr("{0}"));\n'.format(command))
    file.write('}\n\n')

    for command in desired_commands:
        name = command.findtext('proto/name')
        if name == 'vkGetInstanceProcAddr':
            continue

        # Write function prototype.
        file.write('{} ContextTable::{}('.format(convert_type(command.findtext('proto/type')), name))
        param_index = 0
        for param in command.findall('param'):
            if api := param.get('api'):
                if api == 'vulkansc':
                    continue
            param_name = param.findtext('name')
            if param_name == 'device' or param_name == 'instance' or param_name == 'physicalDevice' \
                    or param_name == 'pAllocator':
                continue
            if param_index != 0:
                file.write(', ')
            param_str = ''
            for param_part in param.itertext():
                if len(param_part.strip()) != 0:
                    param_str += convert_type(param_part.strip())
                    if len(param_part.strip()) != 1:
                        param_str += ' '
            file.write(param_str.strip())
            param_index += 1
        file.write(') const {\n')

        # Write function body.
        file.write('    return m_{}('.format(name))
        param_index = 0
        for param in command.findall('param'):
            if api := param.get('api'):
                if api == 'vulkansc':
                    continue
            param_name = param.findtext('name')
            if param_name == 'device':
                param_name = 'm_device'
            elif param_name == 'instance':
                param_name = 'm_instance'
            elif param_name == 'physicalDevice':
                param_name = 'm_physical_device'
            elif param_name == 'pAllocator':
                param_name = 'nullptr'
            if param_index != 0:
                file.write(', ')
            file.write(param_name)
            param_index += 1
        file.write(');\n')
        file.write('}\n\n')
    file.write('} // namespace vull::vkb\n')
    file.write('// NOLINTEND\n')

# Generate type header file.
with open('../engine/include/vull/vulkan/Vulkan.hh', 'w') as file:
    # TODO: Infer the includes.
    file.write('''// File generated by tools/gen_vk.py
// NOLINTBEGIN
#pragma once

#include <stddef.h>
#include <stdint.h>

using xcb_connection_t = struct xcb_connection_t;
using xcb_window_t = uint32_t;
using xcb_visualid_t = uint32_t;

namespace vull::vkb {

#if defined(_WIN32)
#define VKAPI_PTR __stdcall
#else
#define VKAPI_PTR
#endif

''')

    # Emit hardcoded constants.
    constant_name_dict = {}
    hardcoded_constants = registry.find('.//enums[@name="API Constants"]')
    for constant in filter(lambda c: not c.get('alias'), hardcoded_constants.findall('enum')):
        constant_name = constant.get('name').lower()
        constant_name = 'k_' + constant_name[3:]
        if constant_name == 'k_false' or constant_name == 'k_true':
            continue
        constant_name_dict[constant.get('name')] = constant_name
        file.write('constexpr {} {} = {};\n'.format(constant.get('type'), constant_name, constant.get('value').lower()))
    file.write('\n')

    # Emit base types.
    file.write('// Base types.\n')
    for type_name, vk_type in sorted(filter(lambda ty: ty[1].get('category') == 'basetype', desired_types),
                                     key=itemgetter(0)):
        if type_name == 'VkBool32':
            continue
        c_type = vk_type.findtext('type')
        file.write('using {} = {};\n'.format(convert_type(type_name), convert_type(c_type)))
    file.write('\n')

    # Emit custom bool wrapper type that allows implicit conversion from bool -> VkBool32 and vice versa.
    file.write('class Bool {\n')
    file.write('    uint32_t m_value;\n\n')
    file.write('public:\n')
    file.write('    Bool() = default;\n')
    file.write('    Bool(bool value) : m_value(value ? 1 : 0) {}\n')
    file.write('    operator bool() const { return m_value == 1; }\n')
    file.write('};\n\n')

    # Emit bitmasks if an enum doesn't exist.
    existing_enums = [convert_type(e.get('name')) for e in
                      filter(lambda e: len(e.findall('enum')) != 0, registry.findall('enums'))]
    file.write('// Bitmasks.\n')
    for type_name, vk_type in sorted(filter(lambda ty: ty[1].get('category') == 'bitmask', desired_types),
                                     key=itemgetter(0)):
        if vk_type.get('alias'):
            continue
        converted_type_name = convert_type(type_name)
        if converted_type_name not in existing_enums or converted_type_name == 'PipelineCacheCreateFlags':
            file.write('using {} = {};\n'.format(converted_type_name, convert_type(vk_type.findtext('type'))))
    file.write('\n')

    # Emit handles.
    file.write('// Handles.\n')
    for type_name, vk_type in sorted(filter(lambda ty: ty[1].get('category') == 'handle', desired_types),
                                     key=itemgetter(0)):
        file.write('using {0} = struct {0}_T *;\n'.format(convert_type(type_name)))
    file.write('\n')

    # Emit enums.
    file.write('// Enums.\n')
    for type_name, vk_type in sorted(filter(lambda ty: ty[1].get('category') == 'enum', desired_types),
                                     key=itemgetter(0)):
        definition = registry.find('.//enums[@name="{}"]'.format(type_name))
        if definition is None or len(definition.findall('enum')) == 0:
            # Don't generate empty enums, a bitmask will already have been generated.
            continue
        converted_type_name = convert_type(type_name)
        bitwidth = definition.get('bitwidth') or '32'
        file.write('enum class {} {}{{\n'.format(converted_type_name, ': uint64_t ' if bitwidth == '64' else ''))

        # Emit None enumerant if needed.
        if converted_type_name.endswith('Flags'):
            file.write('    None = 0,\n')

        # Build a subtraction name that will be used to remove bits of the enum name from the enumerants.
        is_extension_enum = False
        subtraction_type_name = type_name.replace('FlagBits', '')
        for vendor in [v.get('name') for v in registry.findall('tags/tag')]:
            if subtraction_type_name.endswith(vendor):
                is_extension_enum = True
                subtraction_type_name = subtraction_type_name[:-len(vendor)]
                break

        for enumerant in definition.findall('enum'):
            if enumerant.get('alias'):
                # Ignore alias enumerants, we want to force upgrade to the core version.
                continue

            # One of these must be set for the enumerant.
            bitpos = enumerant.get('bitpos')
            value = enumerant.get('value')

            # Convert enumerant name, from e.g. VK_FRONT_FACE_COUNTER_CLOCKWISE to CounterClockwise.
            name = enumerant.get('name').title().replace('_', '')
            name = name.replace(subtraction_type_name, '')
            name = name.replace('Vk', '')
            name = name.replace('Bit', '')

            for vendor in [v.get('name') for v in registry.findall('tags/tag')]:
                if name.endswith(vendor.title()):
                    name = name[:name.index(vendor.title())]
                    if not is_extension_enum:
                        name += vendor

            # After simplifying the name, the first character may now be a digit (for example VK_IMAGE_TYPE_1D turns
            # into 1D), which isn't a valid identifier so we prefix with an underscore.
            if name[0].isdigit():
                name = '_' + name
            final_value = value or '1{1} << {0}{1}'.format(bitpos, 'ull' if bitwidth == '64' else 'u')
            file.write('    {} = {},\n'.format(name, final_value))
        file.write('};\n')

        # Emit operators for flags.
        if 'FlagBits' in type_name:
            underlying_type_string = 'uint{}_t'.format(bitwidth)
            file.write('inline constexpr {0} operator&({0} lhs, {0} rhs) {{\n'.format(converted_type_name))
            file.write('    return static_cast<{0}>(static_cast<{1}>(lhs) & static_cast<{1}>(rhs));\n'.format(
                converted_type_name, underlying_type_string))
            file.write('}\n')
            file.write('inline constexpr {0} operator|({0} lhs, {0} rhs) {{\n'.format(converted_type_name))
            file.write('    return static_cast<{0}>(static_cast<{1}>(lhs) | static_cast<{1}>(rhs));\n'.format(
                converted_type_name, underlying_type_string))
            file.write('}\n')
        file.write('\n')

    # Emit function pointers, structs, and unions.
    file.write('// Structs and unions.\n')
    for type_name, vk_type in filter(
            lambda ty: ty[1].get('category') == 'funcpointer' or ty[1].get('category') == 'struct' or ty[1].get(
                'category') == 'union',
            desired_types):
        if vk_type.get('alias'):
            continue
        if vk_type.get('category') == 'funcpointer':
            text = ''.join(map(convert_type, vk_type.itertext()))
            text = ''.join(map(lambda x: convert_type(x) + ' ', text.split(' ')))
            file.write('{}\n\n'.format(text))
            continue
        file.write('{} {} {{\n'.format(vk_type.get('category'), convert_type(type_name)))
        for member in vk_type.findall('member'):
            if api := member.get('api'):
                if api == 'vulkansc':
                    continue
            member_text = ''
            for part in member.iter():
                if part.tag != 'comment' and part.text:
                    if part.text in constant_name_dict:
                        member_text += constant_name_dict[part.text]
                    else:
                        member_text += (convert_type(part.text).strip())
                    member_text += ' '
                if part.tag != 'member':
                    if tail := part.tail:
                        member_text += tail.strip()

            file.write('    {};\n'.format(member_text.strip()))
        file.write('};\n\n')

    # Emit command function pointers.
    file.write('// Command function pointers.\n')
    for command in desired_commands:
        file.write('using PFN_{} = {} (*)('.format(command.findtext('proto/name'),
                                                   convert_type(command.findtext('proto/type'))))
        param_index = 0
        for param in command.findall('param'):
            if api := param.get('api'):
                if api == 'vulkansc':
                    continue
            if param_index != 0:
                file.write(', ')
            param_str = ''
            for param_part in param.itertext():
                if len(param_part.strip()) != 0:
                    param_str += convert_type(param_part.strip())
                    if len(param_part.strip()) != 1:
                        param_str += ' '
            file.write(param_str.strip())
            param_index += 1
        file.write(');\n')
    file.write('\n} // namespace vull::vkb\n')
    file.write('// NOLINTEND\n')
