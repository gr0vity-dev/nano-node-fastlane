#!/bin/bash

# This script creates the next tag for the current branch by incrementing the version_pre_release by 1
# A new tag is only created if a new commit has been detected compared to the previous tag.
# The tag has the following format V${current_version_major}.${current_version_minor}${branch_name}
# ${branch_name} is converted to "DB" if the script operates on develop branch. (e.g first tag for V26: V26.0DB1)
# if -c flag is provided, version_pre_release in CMakeLists.txt is incremented and a new tag is created and pushed to origin
# if -o is provided, "build_tag" , "version_pre_release" and "tag_created" are written to file
# If it executes on a release-branch :
#    --> if there  is no new commit, the same tag is generated again
#    --> If there is a new commit compared to the previous tag, we would increment the minor version by 1 and build the new binaries & docker images

#!/bin/bash

set -e
set -x 
output=""
create=false
tag_created="false"

while getopts ":o:c" opt; do
  case ${opt} in
    o )
      output=$OPTARG
      ;;
    c )
      create=true
      ;;
    \? )
      echo "Invalid Option: -$OPTARG" 1>&2
      exit 1
      ;;
    : )
      echo "Invalid Option: -$OPTARG requires an argument" 1>&2
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))


get_tag_suffix() {
    local branch_name=$1
    local version_major=$2
    local tag_suffix=${branch_name//[^a-zA-Z0-9]/_}

    if [[ "$branch_name" == "develop" ]]; then
        tag_suffix="DB"
    fi

    echo $tag_suffix
}

get_next_tag_number() {
    local last_tag_number=$1
    local increment=$2
    echo $((last_tag_number + increment))
}

get_next_minor_version() {
    local current_minor=$1
    local increment=$2
    echo $((current_minor + increment))
}

get_new_release_tag() {
    local version_major=$1
    local next_minor=$2
    echo "V${version_major}.${next_minor}"
}

get_new_other_tag() {
    local base_version=$1
    local next_tag_number=$2
    echo "${base_version}${next_tag_number}"
}

update_output_file() {   
    local new_tag=$1
    local next_number=$2
    local tag_created=$3
    local tag_type=$4

    if [[ -n "$output" ]]; then
        echo "build_tag =$new_tag" > $output
        echo "$tag_type =$next_number" >> $output
        echo "tag_created =$tag_created" >> $output
    fi
}

update_cmake_lists() {
    local tag_type=$1
    local next_number=$2
    local variable_to_update=""

    if [[ "$tag_type" == "version_pre_release" ]]; then
        variable_to_update="CPACK_PACKAGE_VERSION_PRE_RELEASE"
    elif [[ "$tag_type" == "version_minor" ]]; then
        variable_to_update="CPACK_PACKAGE_VERSION_MINOR"
    fi

    if [[ -n "$variable_to_update" ]]; then
        echo "Update ${variable_to_update} to $next_number"
        sed -i.bak "s/set(${variable_to_update} \"[0-9]*\")/set(${variable_to_update} \"${next_number}\")/g" CMakeLists.txt
        rm CMakeLists.txt.bak        
    fi
    git add CMakeLists.txt
}

function create_commit() {
    git diff --cached --quiet
    local has_changes=$? # store exit status of the last command
    if [[ $has_changes -eq 0 ]]; then # no changes
        echo "No changes to commit"
        echo "false"
    else # changes detected
        git commit -m "Update CMakeLists.txt"
        echo "true"
    fi
}


# Fetch all existing tags
git fetch --tags -f

# Fetch the last commit hash of the current branch
current_commit_hash=$(git rev-parse HEAD)

# Fetch branch name
branch_name=$(git rev-parse --abbrev-ref HEAD)

# Determine if it's a release branch or not
is_release_branch=$(echo "$branch_name" | grep -q "releases/v$current_version_major" && echo true || echo false)


# Fetch major and minor version numbers from CMakeLists.txt
current_version_major=$(grep "CPACK_PACKAGE_VERSION_MAJOR" CMakeLists.txt | grep -o "[0-9]\+")
current_version_minor=$(grep "CPACK_PACKAGE_VERSION_MINOR" CMakeLists.txt | grep -o "[0-9]\+")

# Initialize tag suffix and next number and increment
tag_suffix=""
next_number=0
increment=1

if [[ $is_release_branch == true ]]; then     

    tag_type="version_minor"    
    # Find existing tags for the release branch
    existing_release_tags=$(git tag --list "V${current_version_major}.*" | grep -E "V${current_version_major}\.[0-9]+$" || true)

    # Check if any tag exists for the release branch
    if [[ -z "$existing_release_tags" ]]; then
        # No tag exists yet, use current minor version without incrementing
        tag_created="true"
        increment=0
    else
        # Some tags already exist
        # Get the commit hash of the latest tag
        last_tag=$(echo "$existing_release_tags" | sort -V | tail -n1)
        last_tag_commit_hash=$(git rev-list -n 1 $last_tag)

        if [[ "$current_commit_hash" == "$last_tag_commit_hash" ]]; then
            # The commit hash of the HEAD is the same as the last tag, hence no new commits. No need to increment
            tag_created="true"
            increment=0
        else
            # There is a new commit, hence increment the minor version by 1
            tag_created="true"
            increment=1
        fi        
    fi    
    next_number=$(get_next_minor_version $current_version_minor $increment)
    new_tag=$(get_new_release_tag $current_version_major $next_number)

else
    # Non-release branches handling
    tag_type="version_pre_release"
    
    tag_suffix=$(get_tag_suffix $branch_name $current_version_major)
    base_version="V${current_version_major}.${current_version_minor}${tag_suffix}"
    existing_tags=$(git tag --list "${base_version}*" | grep -E "${base_version}[0-9]+$" || true)

    if [[ -n "$existing_tags" ]]; then
        last_tag=$(echo "$existing_tags" | sort -V | tail -n1)
        last_tag_number=$(echo "$last_tag" | awk -F"${tag_suffix}" '{print $2}')
        last_tag_commit_hash=$(git rev-list -n 2 $last_tag | tail -n 1) #ignore the commit that updates the version_pre_release
        
        if [[ "$current_commit_hash" == "$last_tag_commit_hash" ]]; then
            echo "No new commits since the last tag. No new tag will be created."
            tag_created="false"
        else
            tag_created="true"
            next_number=$(get_next_tag_number $last_tag_number $increment)
        fi
    else
        tag_created="true"
        next_number=1 #replace the default 99       
    fi
    new_tag=$(get_new_other_tag $base_version $next_number)        
fi

update_output_file $new_tag $next_number $tag_created $tag_type

# Skip tag creation if no new commits
if [[ "$tag_created" == "true" ]]; then
    echo "$new_tag"
else
    exit 0
fi

if [[ $create == true ]]; then
    # Stash current changes
    git config user.name "${GITHUB_ACTOR}"
    git config user.email "${GITHUB_ACTOR}@users.noreply.github.com"

    # Update variable in CMakeLists.txt
    update_cmake_lists "$tag_type" "$next_number"

    commit_made=$(create_commit)

    git tag -fa "$new_tag" -m "This tag was created with generate_next_git_tag.sh"
    git push origin "$new_tag" -f
    echo "The tag $new_tag has been created and pushed."

    # If it's a release branch, also push the commit to the branch
    if [[ $is_release_branch == true ]]; then
        git push origin "$branch_name" -f
        echo "The commit has been pushed to the $branch_name branch."
    fi

    # Only reset local branch if a commit was made and it's not a "releases" branch.
    if [[ "$commit_made" == "true" && $is_release_branch == false ]]; then
        git reset --hard HEAD~1
        echo "The commit used for the tag does not exist on any branch."
    fi
fi