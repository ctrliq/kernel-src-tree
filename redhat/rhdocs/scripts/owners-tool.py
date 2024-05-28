import argparse
import importlib
import itertools
import sys

def eprint(*args, **kwargs):
    kwargs['file'] = sys.stderr
    print(*args, **kwargs)

def import_modules(*args):
    result = []
    missing = []
    for name in args:
        pip_name = name
        if isinstance(name, tuple):
            name, pip_name = name
        try:
            result.append(importlib.import_module(name))
        except ModuleNotFoundError:
            missing.append(pip_name)
    if missing:
        eprint('ERROR: missing Python modules. Please run one of the following:')
        names = ' '.join('python3-' + name for name in missing)
        eprint('  (1) dnf install {}'.format(names))
        for i, name in enumerate(missing):
            eprint('  {} pip install {}'.format('(2)' if i == 0 else '   ', name))
        sys.exit(1)
    return result

jinja2, jsonschema, yaml = import_modules('jinja2', 'jsonschema', ('yaml', 'pyyaml'))


class BaseCommand:
    overview = ''

    def __init__(self, subparsers):
        self.name = type(self).__name__.lower()
        if self.name.startswith('command'):
            self.name = self.name[7:]
        self.parser = subparsers.add_parser(self.name, help=self.overview)
        self.parser.set_defaults(obj=self)
        self.add_arguments()

    def add_arguments(self):
        pass

    def handle(self, args):
        pass

    def add_argument_owners(self):
        self.parser.add_argument('owners', type=self.load_yaml,
                                 help='path to owners.yaml')

    def add_argument_owners_schema(self):
        self.parser.add_argument('owners_schema', type=self.load_yaml,
                                 help='path to owners-schema.yaml')

    def load_yaml(self, path):
        try:
            with open(path) as f:
                return yaml.safe_load(f)
        except IOError as e:
            raise argparse.ArgumentTypeError(str(e))


class CommandVerify(BaseCommand):
    overview = 'Verify owners.yaml for correctness.'

    def add_arguments(self):
        self.add_argument_owners()
        self.add_argument_owners_schema()

    def handle(self, args):
        try:
            jsonschema.validate(args.owners, args.owners_schema,
                                format_checker=jsonschema.draft4_format_checker)
        except jsonschema.exceptions.ValidationError as e:
            print(e.validator)
            msg = e.message
            if e.validator == 'type' and e.instance is None:
                msg = 'Value cannot be empty. Either specify the value or omit the key "{}" completely.'.format(e.path[-1])
            try:
                msg = e.schema['message'][e.validator]
            except KeyError:
                pass
            loc = ' -> '.join(map(str, e.absolute_path))
            try:
                if e.absolute_path[0] == 'subsystems':
                    subsys = args.owners['subsystems'][e.absolute_path[1]]['subsystem']
                    new_loc = ['subsystem "{}"'.format(subsys)]
                    new_loc.extend(list(e.absolute_path)[2:])
                    loc = ' -> '.join(map(str, new_loc))
            except (IndexError, KeyError):
                pass

            eprint('ERROR: owners.yaml: {}: {}'.format(loc, msg))
            return False

        errors = 0
        for subsys in args.owners['subsystems']:
            if not subsys.get('reviewers'):
                continue

            # Check for duplicates between maintainers and reviewers. Also check for
            # a person being included twice with the same email but different gluser
            # (or vice versa).
            emails = set()
            glusers = set()
            for person in itertools.chain(subsys['maintainers'], subsys['reviewers']):
                if person['email'] in emails or person['gluser'] in glusers:
                    eprint('ERROR: owners.yaml: subsystem "{}": '.format(subsys['subsystem']), end='')
                    eprint('Duplicate maintainer/reviewer entry for "{}".'.format(person['name']))
                    errors += 1
                emails.add(person['email'])
                glusers.add(person['gluser'])

            # requiredApproval can be true only if there are at least
            # 3 maintainers/reviewers.
            if subsys.get('requiredApproval') and len(emails) < 3:
                eprint('ERROR: owners.yaml: subsystem "{}": '.format(subsys['subsystem']), end='')
                eprint('requiredApproval can be set only if there are at least 3 maintainers and/or reviewers.')
                errors += 1

        return errors == 0


class CommandDoc(BaseCommand):
    overview = 'Generate documentation from owners.yaml schema.'

    def add_arguments(self):
        self.add_argument_owners_schema()

    def format_description(self, content, optional=False):
        description = content.get('description', '')
        if optional:
            description = '{}{}Optional.'.format(description, ' ' if description else '')
        if 'enum' in content:
            description = '{}{}Allowed values: {}.'.format(
                            description,
                            ' ' if description else '',
                            ', '.join(f'`{c}`' for c in content['enum']))
        return f' footnote:[{description}]' if description else ''

    def format_properties(self, obj, depth=0, is_array=False):
        result = []
        required = obj.get('required', ())
        for prop, content in obj['properties'].items():
            description = self.format_description(content, prop not in required)
            prop_type = content['type']
            if prop_type == 'string':
                result.append(f'{prop}: __value__{description}')
            elif prop_type == 'boolean':
                result.append(f'{prop}: __true / false__{description}')
            elif prop_type == 'object':
                result.append(f'{prop}:{description}')
                result.extend(self.format_properties(content, depth + 1))
            elif prop_type == 'array':
                result.append(f'{prop}:{description}')
                item_type = content['items']['type']
                if item_type == 'string':
                    result.append('  - __value...__' +
                                  self.format_description(content['items']))
                elif item_type == 'object':
                    result.extend(self.format_properties(content['items'], depth + 1, True))
                else:
                    eprint(f'Usupported array type: {item_type}')
                    sys.exit(1)
            else:
                eprint(f'Unsupported property type: {prop_type}')
                sys.exit(1)
        fmt_result = []
        indent = '  ' * depth
        for line in result:
            if is_array:
                fmt_result.append(f'{indent}- {line}')
                indent += '  '
                is_array = False
            else:
                fmt_result.append(indent + line)
        return fmt_result

    def handle(self, args):
        print('''---
title: The Format of owners.yaml
weight: 100
---

[subs="+quotes,+macros"]
----''')
        print('\n'.join(self.format_properties(args.owners_schema)))
        print('----')
        return True


class ChainableUndefined(jinja2.Undefined):
    def __getattr__(self, name):
        return self


class CommandConvert(BaseCommand):
    overview = 'Convert owners.yaml using a template.'

    def add_arguments(self):
        self.add_argument_owners()
        self.parser.add_argument('template',
                                 help='path to the template')

    def handle(self, args):
        env = jinja2.Environment(loader=jinja2.FileSystemLoader('.'),
                                 undefined=ChainableUndefined,
                                 trim_blocks=True)
        try:
            template = env.get_template(args.template)
        except jinja2.TemplateNotFound as e:
            eprint('ERROR: template not found:', str(e))
            return False
        print(template.render(args.owners), end='')
        return True


parser = argparse.ArgumentParser(description='Verify and convert owners.yaml.')
subparsers = parser.add_subparsers(dest='command', title='available commands',
                                   metavar='COMMAND')
for name, cls in list(globals().items()):
    if name.startswith('Command'):
        cls(subparsers)
args = parser.parse_args()
if not args.command:
    parser.print_help()
    sys.exit(1)
success = args.obj.handle(args)
sys.exit(0 if success else 1)
