#!/bin/bash
set -e
set -a

scripts="$PWD/ci"
CI_BRANCH=$(git rev-parse --abbrev-ref HEAD)
tags=()

if [ -n "$CI_TAG" ]; then
    tags+=("$CI_TAG")
elif [ -n "$CI_BRANCH" ]; then
    CI_TAG=$CI_BRANCH
    tags+=("$CI_BRANCH")
fi


if [[ "$NETWORK" = "LIVE" ]]; then
    echo "Live"
    network_tag_suffix=''
    network="live"
elif [[ "$NETWORK" = "BETA" ]]; then
    echo "Beta"
    network_tag_suffix="-beta"
    network="beta"
elif [[ "$NETWORK" = "TEST" ]]; then
    echo "Test"
    network_tag_suffix="-test"
    network="test"
fi

docker_image_name="nanocurrency/nano${network_tag_suffix}"
ghcr_image_name="ghcr.io/${GITHUB_REPOSITORY}/nano${network_tag_suffix}"

docker_build()
{
    ci_version_pre_release="OFF"
    if [[ -n "${CI_VERSION_PRE_RELEASE}" ]]; then
        ci_version_pre_release="$CI_VERSION_PRE_RELEASE"
    fi

    build_docker_image "$ci_version_pre_release"
    
    for tag in "${tags[@]}"; do
        local sanitized_tag=$(sanitize_tag "$tag")
        tag_docker_image "$sanitized_tag"
    done
}

build_docker_image() {
    local ci_version_pre_release="$1"
    "$scripts"/build-docker-image.sh docker/node/Dockerfile "$docker_image_name" \
        --build-arg NETWORK="$network" \
        --build-arg CI_BUILD=true \
        --build-arg CI_VERSION_PRE_RELEASE="$ci_version_pre_release" \
        --build-arg CI_TAG="$CI_TAG"
}

sanitize_tag() {
    local tag="$1"
    # Sanitize docker tag
    # https://docs.docker.com/engine/reference/commandline/tag/
    tag="$(printf '%s' "$tag" | tr -c '[a-z][A-Z][0-9]_.-' -)"
    echo "$tag"
}

tag_docker_image() {
    local tag="$1"
    if [ "$tag" != "latest" ]; then
        docker tag "$docker_image_name" "${docker_image_name}:$tag"
        docker tag "$ghcr_image_name" "${ghcr_image_name}:$tag"
    fi
}

docker_deploy_env()
{
    docker_login "nanoreleaseteam" "$DOCKER_PASSWORD" 
    local images=(
        "nanocurrency/nano-env:base"
        "nanocurrency/nano-env:gcc"
        "nanocurrency/nano-env:clang"
    )
    deploy_env_images "${images[@]}"
}

docker_deploy()
{
    docker_login "nanoreleaseteam" "$DOCKER_PASSWORD"
    if [[ "$NETWORK" = "LIVE" ]]; then
        deploy_tags "nanocurrency" "env|ghcr.io|none|latest"
    else
        deploy_tags "nanocurrency" "env|ghcr.io|none"
    fi

}

ghcr_deploy()
{    
    deploy_tags "ghcr.io/${GITHUB_REPOSITORY}" "env|none"
}

ghcr_deploy_env()
{        
    local images=(
        "ghcr.io/${GITHUB_REPOSITORY}/nano-env:base"
        "ghcr.io/${GITHUB_REPOSITORY}/nano-env:gcc"
        "ghcr.io/${GITHUB_REPOSITORY}/nano-env:clang"
        "ghcr.io/${GITHUB_REPOSITORY}/nano-env:rhel"
    )
    deploy_env_images "${images[@]}"   
}

docker_login()
{
    local username=$1
    local password=$2

    if [ -z "$password" ]; then
        echo "\$DOCKER_PASSWORD or \$GHCR_TOKEN environment variable required"
        exit 1
    fi

    echo "$password" | docker login -u "$username" --password-stdin
}

push_docker_image()
{
    local image_name=$1
    "$scripts"/custom-timeout.sh 30 docker push "$image_name"
}

deploy_env_images()
{
    local images=("$@")

    for image in "${images[@]}"; do
        push_docker_image "$image"
    done

    echo "Deployed nano-env"
}

deploy_tags()
{
    local repo=$1
    local exclude_pattern=$2
    local tags=$(docker images --format '{{.Repository}}:{{.Tag }}' | grep "$repo" | grep -vE "$exclude_pattern")

    for tag in $tags; do
        push_docker_image "$tag"
    done

    echo "$repo with tags ${tags//$'\n'/' '} deployed"
}
