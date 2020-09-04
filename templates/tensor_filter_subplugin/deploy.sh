#!/bin/bash
##
## @file deploy.sh
## @author MyungJoo Ham <myungjoo.ham@gmail.com>
## @date Oct 11 2019
## @brief This creates a new git repo with the given template.
TARGET=$(pwd)
BASEPATH=`dirname "$0"`
BASENAME=`basename "$0"`


if [[ $# -lt 2 ]]
then
	printf "usage: ${BASENAME} <name> <path to create>\n\n"
	printf "    This creates a new git repo at <path to create> with the\n"
	printf "  template code for a tensor-filter subplugin, \"<name>\"\n\n"
	exit 1
fi

name="$1"
path="$2"

regex="\W"
if [[ $name =~ $regex ]]
then
	printf "The name \"$1\" contains a whitespace. Cannot proceed.\n\n"
	exit 1
fi
regex="^[a-zA-Z0-9_\-]+$"
if [[ ! $name =~ $regex ]]
then
	printf "Please provide name with A-Z, a-z, -, _, 0-9 only. The name \"$1\" is not such a name. Cannot proceed.\n\n"
	exit 1
fi


if [ -e $2 ]
then
	printf "The path $2 already exists. Please designate a new path.\n\n"
	exit 1
fi

mkdir -p $2
if [ ! -d $2 ]
then
	printf "Failed to create a new directory $2.\n\n"
	exit 1
fi

printf "Initializing a git repo of tensor-filter at $2.\n"
pushd $2
git init
popd

cp -R src packaging meson.build $2/

pushd $2

pushd packaging
mv tensor-filter-TEMPLATE.manifest tensor-filter-${name}.manifest
mv tensor-filter-TEMPLATE.spec.in tensor-filter-${name}.spec

sed -i "s|TEMPLATE|${name}|g" tensor-filter-${name}.spec
popd
sed -i "s|TEMPLATE|${name}|g" meson.build
sed -i "s|TEMPLATE|${name}|g" src/tensor_filter_subplugin.c

git add meson.build src/*.c packaging/*
git commit -m "Initial Tensor-Filter Subplugin Code of ${name}" -m "This is the template code of nnstreamer tensor_filter subplugin"
popd
