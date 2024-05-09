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

jsonschema, yaml = import_modules('jsonschema', ('yaml', 'pyyaml'))


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
                                format_checker=jsonschema.Draft202012Validator.FORMAT_CHECKER)
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
