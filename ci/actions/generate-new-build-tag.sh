#!/bin/bash

# The script is designed to generate the build tags for the 'develop' branch and the latest release branch based on the inputs.
# It's used for managing and automating the versioning in a CI/CD environment.

# Global variables
source_dir="$(pwd)"
git_upstream="origin"
target_previous_release=false
output_file=""
current_version_major=""
current_version_minor=""
current_version_pre_release=""

# Print short usage of the script
print_short_usage() {
    echo "$(basename ${0}) -o <output_file> [OPTIONS]"
    echo "Specify -h to see the options."
}

# Print complete usage of the script
print_usage() {
    echo "$(basename ${0}) -o <output_file> [OPTIONS]"
    echo "ARGUMENTS:"
    echo "  -o <output_file>     Export the variables to an output file (sourcing this script is deprecated)."
    echo
    echo "OPTIONS:"
    echo "  [-h]                 Print this help info."
    echo "  [-s <source_dir>]    Directory that contains the source-code. Default is \$PWD."
    echo "  [-u <git_upstream>]  Name of the git repository upstream. Default is \"${git_upstream}\"."
    echo "  [-r]                 Generates build tag for the latest release branch."
}

# Extract the first item from the list
get_first_item() {
    local list="$1"
    for item in $list; do
        if [[ -n "$item" ]]; then
            echo "$item"
            break
        fi
    done
}

# Output variable to the file
output_variable() {
    if [[ $# -ne 1 ]]; then
        echo "Illegal number of parameters"
        exit 1
    fi
    local var_name="$1"
    local var_value=${!var_name}
    if [[ -n "$output_file" ]]; then
        echo "${var_name}=${var_value}" >> "$output_file"
    fi
}

# Extract version information from the 'develop' branch
extract_develop_version() {
    current_version_major=$(grep -P "(set)(.)*(CPACK_PACKAGE_VERSION_MAJOR)" "${source_dir}/CMakeLists.txt" | grep -oP "([0-9]+)")
    current_version_minor=$(grep -P "(set)(.)*(CPACK_PACKAGE_VERSION_MINOR)" "${source_dir}/CMakeLists.txt" | grep -oP "([0-9]+)")
    current_version_pre_release=$(grep -P "(set)(.)*(CPACK_PACKAGE_VERSION_PRE_RELEASE)" "${source_dir}/CMakeLists.txt" | grep -oP "([0-9]+)")
}

# Process 'develop' branch
process_develop_branch() {
    # List of all develop-build (DB) tags in reverse version order.
    sorted_release_tags=$(git tag | sort -V -r | grep -E "^(V(${current_version_major})\.(${current_version_minor})(DB[0-9]+))$" || true)
}

# Process release branch
process_release_branch() {
    previous_release_major=$(( current_version_major - 1 ))
    # List of official release tags in reverse order.
    sorted_release_tags=$(git tag | sort -V -r | grep -E "^(V(${previous_release_major})\.([0-9]+))$" || true)
    if [[ -z "$sorted_release_tags" ]]; then
        # Previous release minor is 0 if no official release is found in the branch.
        previous_release_minor=0
    else
        # Previous release minor version is increased by 1 because next develop-build should go on unreleased minor versions.
        last_minor_release=$(get_first_item "$sorted_release_tags")
        last_minor=$(echo "$last_minor_release" | grep -oP "\.([0-9]+)" | grep -oP "[0-9]+")
        previous_release_minor=$(( last_minor + 1 ))
    fi
    # List of tags in reverse version order that may or many not include develop-build (DB) tags.
    sorted_release_tags=$(git tag | sort -V -r | grep -E "^(V(${previous_release_major})\.(${previous_release_minor})(DB[0-9]+)?)$" || true)
    # The reference release branch in Git repository should follow the pattern 'releases/vXY'.
    release_branch="releases/v${previous_release_major}"
    output_variable release_branch
}

# Check if the head of the branch already has a tag
check_branch_head_tag() {
    # This code block looks for the branch head in order to know if there is already a tag for it. There is no need
    # to generate a new tag if the develop-build has already been created.
    develop_head=""
    if [[ $target_previous_release == false ]]; then
        develop_head=$(git rev-parse "${git_upstream}/develop")
    else
        develop_head=$(git rev-parse "${git_upstream}/${release_branch}")
    fi
    tag_head=$(git rev-list "$last_tag" | head -n 1)

    if [[ "$develop_head" == "$tag_head" ]]; then
        echo "Error: no new commits for the develop build, the develop (or release) branch head matches the latest DB tag head!"
        exit 2
    fi
}

# Generate the build tag
generate_build_tag() {
    latest_version_pre_release=$(echo "$last_tag" | grep -oP "(DB[0-9]+)" | grep -oP "[0-9]+")
    version_pre_release=$(( latest_version_pre_release + 1 ))
    if [[ $target_previous_release == false ]]; then
        build_tag="V${current_version_major}.${current_version_minor}DB${version_pre_release}"
    else
        build_tag="V${previous_release_major}.${previous_release_minor}DB${version_pre_release}"
    fi
    output_variable version_pre_release
    output_variable build_tag
}

# Main function of the script
main() {
    while getopts 'hs:u:ro:' OPT; do
        case "${OPT}" in
        h)
            print_usage
            exit 0
            ;;
        s)
            source_dir="${OPTARG}"
            if [[ ! -d "$source_dir" ]]; then
                echo "Error: invalid source directory"
                exit 1
            fi
            ;;
        u)
            git_upstream="${OPTARG}"
            if [[ -z "$git_upstream" ]]; then
                echo "Error: invalid git upstream"
                exit 1
            fi
            ;;
        r)
            target_previous_release=true
            ;;
        o)
            output_file="${OPTARG}"
            if [[ -f "$output_file" ]]; then
                echo "Error: the provided output_file already exists"
                exit 1
            fi
            ;;
        *)
            print_usage >&2
            exit 1
            ;;
        esac
    done

    if [[ -z "$output_file" ]]; then
        echo "Error: invalid file name for exporting the variables"
        print_short_usage >&2
        exit 1
    fi

    set -o nounset
    set -o xtrace

    fetch_version_info
    check_branch_version_info
    cd "$source_dir"

    if [[ $target_previous_release == false ]]; then
        process_develop_branch
    else
        process_release_branch
    fi

    cd -
    check_branch_head_tag
    cd "$source_dir"
    generate_build_tag

    set +o nounset
    set +o xtrace
}

main "$@"
