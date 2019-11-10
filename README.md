# subl.gdb

** Link time plugin to modify sublime.project files to synchronise binaries targets for debugging with https://github.com/quarnster/SublimeGDB **

## Prerequisites
  [maiken](https://github.com/Dekken/maiken)
  [parse.json](https://github.com/mkn/parse.json)

## Usage

```yaml
mod:
- name: subl.gdb
  link:
    project_file: $str  # defaults see "Defaults"

```

## Building

  *nix gcc:

    mkn clean build -dtOa "-fPIC" -l "-pthread -ldl"


## Testing

  *nix gcc:

    mkn clean build -dtOa "-fPIC" -l "-pthread -ldl" -p test run

##Defaults

  project_file will attempt the following order
    $PWD/.sublime-project

  if no file is found an error will be raised
