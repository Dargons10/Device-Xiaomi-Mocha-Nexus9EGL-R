#!/bin/sh

rootdirectory="$PWD"
<<<<<<< HEAD
dirs=" bionic/ system/core frameworks/native frameworks/base external/selinux"
=======
dirs=" bionic/libm frameworks/native frameworks/av external/selinux system/core system/extras"
>>>>>>> faf415f (libstagefrighthw patched)


for dir in $dirs ; do
	cd $rootdirectory
	cd $dir
	echo "Cleaning $dir patches..."
	git checkout -- . && git clean -df
done

echo "Done!"
cd $rootdirectory
