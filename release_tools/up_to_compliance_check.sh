#!/bin/bash

# Created by argbash-init v2.5.0
# ARGBASH_SET_DELIM([ ])
# ARGBASH_SET_INDENT([    ])
# ARG_OPTION_STACKING([none])
# ARG_RESTRICT_VALUES([none])
# ARG_OPTIONAL_BOOLEAN([clean],[c],[Clean the repository before proceeding. Use with care, as you will lose all uncommitted local modifications and all untracked files.],[off])
# ARG_HELP([Compile OpenSCAP, perform tests and execute ABI check.])
# DEFINE_SCRIPT_DIR([])
# ARGBASH_GO()
# needed because of Argbash --> m4_ignore([
### START OF CODE GENERATED BY Argbash v2.5.0 one line above ###
# Argbash is a bash code generator used to get arguments parsing right.
# Argbash is FREE SOFTWARE, see https://argbash.io for more info

die()
{
    local _ret=$2
    test -n "$_ret" || _ret=1
    test "$_PRINT_HELP" = yes && print_help >&2
    echo "$1" >&2
    exit ${_ret}
}


# THE DEFAULTS INITIALIZATION - OPTIONALS
_arg_clean=off

print_help ()
{
    printf "%s\n" "Compile OpenSCAP, perform tests and execute ABI check."
    printf 'Usage: %s [-c|--(no-)clean] [-h|--help]\n' "$0"
    printf "\t%s\n" "-c,--clean,--no-clean: Clean the repository before proceeding. Use with care, as you will lose all uncommitted local modifications and all untracked files. (off by default)"
    printf "\t%s\n" "-h,--help: Prints help"
    echo
    echo 'Short options stacking mode is not supported.'
}

parse_commandline ()
{
    while test $# -gt 0
    do
        _key="$1"
        case "$_key" in
            -c|--no-clean|--clean)
                _arg_clean="on"
                test "${1:0:5}" = "--no-" && _arg_clean="off"
                ;;
            -h|--help)
                print_help
                exit 0
                ;;
            *)
                _PRINT_HELP=yes die "FATAL ERROR: Got an unexpected argument '$1'" 1
                ;;
        esac
        shift
    done
}

parse_commandline "$@"

# OTHER STUFF GENERATED BY Argbash
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || die "Couldn't determine the script's running directory, which probably matters, bailing out" 2

### END OF CODE GENERATED BY Argbash (sortof) ### ])
# [ <-- needed because of Argbash


. "$script_dir/release.sh"

build_stderr=build-stderr.log
build_stdout=build-stdout.log

if ["$_arg_clean" = "on"]; then
    echo Cleaning the repository...
    clean_repository
    clean_repository_aggressively
fi


echo Checking whether the repository is clean...
check_for_clean_repo
echo Checking appropriate CPEs...
check_cpe
echo Building and executing tests...
execute_local_tests > "$build_stdout" 2> "$build_stderr" || die "The build or tests failed, check '$build_stdout' and '$build_stderr' for errors."
echo Arranging the API check...
check_abi

# ] <-- needed because of Argbash
