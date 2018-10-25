#!/bin/bash
#set -x
set -o nounset
CURRENT_PATH=`pwd`
cd ${CURRENT_PATH}/..
export SVS_ROOT=$PWD
export PREFIX_ROOT=${SVS_ROOT}/svs_extends/
export THIRD_ROOT=${SVS_ROOT}/3rd_party/

mkdir -p ${PREFIX_ROOT}/lib
ln -s ${PREFIX_ROOT}/lib ${PREFIX_ROOT}/lib64

echo "------------------------------------------------------------------------------"
echo " SVS_ROOT exported as ${SVS_ROOT}"
echo "------------------------------------------------------------------------------"

quit()
{
    QUIT=$1
}

build_ace()
{
    module_pack="ACE-6.5.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the ace package from server\n"
        wget http://download.dre.vanderbilt.edu/previous_versions/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd ACE_wrappers
    export ACE_ROOT=${THIRD_ROOT}/ACE_wrappers
    export LD_LIBRARY_PATH=/usr/local/lib:${ACE_ROOT}/lib
    
    echo -e "#ifndef _CONFIG_H_\n#define _CONFIG_H_\n#include \"ace/config-linux.h\"\n#endif" > ${ACE_ROOT}/ace/config.h
    
    ln -s  ${ACE_ROOT}/include/makeinclude/platform_linux.GNU ${ACE_ROOT}/include/makeinclude/platform_macros.GNU 
    
    echo "INSTALL_PREFIX=${PREFIX_ROOT}" >> ${ACE_ROOT}/include/makeinclude/platform_linux.GNU
                
    make && make install
    
    if [ 0 -ne ${?} ]; then
        echo "build ace fail!\n"
        return 1
    fi
    
    return 0
}

build_osip2()
{

    module_pack="libosip2-5.0.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the libosip2 package from server\n"
        wget https://ftp.gnu.org/gnu/osip/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd libosip*
    ./configure --prefix=${PREFIX_ROOT} 
                
    if [ 0 -ne ${?} ]; then
        echo "configure libosip2 fail!\n"
        return 1
    fi
                
    make && make install
    
    if [ 0 -ne ${?} ]; then
        echo "build libosip2 fail!\n"
        return 1
    fi
    
    return 0
}
build_exosip()
{
    module_pack="libexosip2-5.0.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the libexosip2 package from server\n"
        wget http://download.savannah.nongnu.org/releases/exosip/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd libexosip2*

    ./configure --prefix=${PREFIX_ROOT} --enable-openssl=no
                
    if [ 0 -ne ${?} ]; then
        echo "configure libexosip2 fail!\n"
        return 1
    fi
    
    make clean  
    make && make install
    
    if [ 0 -ne ${?} ]; then
        echo "build libexosip2 fail!\n"
        return 1
    fi
    
    return 0
}
build_libevent()
{
    module_pack="libevent-2.1.8-stable.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the libevent package from server\n"
        wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd libevent*
    ./configure --prefix=${PREFIX_ROOT}
                
    if [ 0 -ne ${?} ]; then
        echo "configure libevent fail!\n"
        return 1
    fi
                
    make && make install
    
    if [ 0 -ne ${?} ]; then
        echo "build libevent fail!\n"
        return 1
    fi
    
    return 0
}

build_vms()
{
    cd ${PREFIX_ROOT}/vms/
                
    make
    
    if [ 0 -ne ${?} ]; then
        echo "build vms fail!\n"
        return 1
    fi
    
    return 0
}


build_extend_modules()
{
    build_ace
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_osip2
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_exosip
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_libevent
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_vms
    if [ 0 -ne ${?} ]; then
        return 1
    fi

    return 0
}

build_access_control()
{
    cd ${SVS_ROOT}/svs_cc/svs_access_control/
    make -j4
    if [ 0 -ne ${?} ]; then
        echo "build the access control module fail!\n"
        return 1
    fi
    return 0
}
build_mu_stream()
{
    cd ${SVS_ROOT}/svs_mu/svs_mu_stream/
    make -j4
    if [ 0 -ne ${?} ]; then
        echo "build the mu stream module fail!\n"
        return 1
    fi

    return 0
}

build_mu_record()
{
    cd ${SVS_ROOT}/svs_mu/svs_mu_record/
    make -j4
    if [ 0 -ne ${?} ]; then
        echo "build the mu record module fail!\n"
        return 1
    fi

    return 0
}

setup()
{  
    build_extend_modules
    if [ 0 -ne ${?} ]; then
        return
    fi 
    build_access_control
    if [ 0 -ne ${?} ]; then
        return
    fi
    build_mu_stream
    if [ 0 -ne ${?} ]; then
        return
    fi
    echo "make the all modules success!\n"
    cd ${SVS_ROOT}
}

package_all()
{

    mkdir -p ${CURRENT_PATH}/svs_server/
    rm -rf ${CURRENT_PATH}/svs_server/*
    
    ####### pack the cc#####################
    
    mkdir -p ${CURRENT_PATH}/svs_server/svs_cc/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_cc/bin/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_cc/lib/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_cc/conf/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_cc/log/
    
    cp -R ${PREFIX_ROOT}/lib/libACE.so* ${CURRENT_PATH}/svs_server/svs_cc/lib/
    cp -R ${PREFIX_ROOT}/lib/libvmsStack64.so ${CURRENT_PATH}/svs_server/svs_cc/lib/
    cp -R ${CURRENT_PATH}/conf/svs_access_control.conf ${CURRENT_PATH}/svs_server/svs_cc/conf/
    cp -R ${CURRENT_PATH}/bin/svs_access_control ${CURRENT_PATH}/svs_server/svs_cc/bin/
    
    ####### pack the mu#####################
    
    mkdir -p ${CURRENT_PATH}/svs_server/svs_mu/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_mu/bin/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_mu/lib/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_mu/conf/
    mkdir -p ${CURRENT_PATH}/svs_server/svs_mu/log/
    
    cp -R ${PREFIX_ROOT}/lib/libACE.so* ${CURRENT_PATH}/svs_server/svs_mu/lib/
    cp -R ${PREFIX_ROOT}/lib/libvmsStack64.so ${CURRENT_PATH}/svs_server/svs_mu/lib/
    cp -R ${CURRENT_PATH}/conf/svs_mu_*.conf ${CURRENT_PATH}/svs_server/svs_mu/conf/
    cp -R ${CURRENT_PATH}/bin/svs_mu_* ${CURRENT_PATH}/svs_server/svs_mu/bin/
    

    cd ${CURRENT_PATH}
    
    cur_date=`date  +%Y%m%d%H%M%S`
    
    tar -zcvf svs_server_${cur_date}.tar.gz svs_server/
    
    rm -rf ${CURRENT_PATH}/svs_server/
}


extend_func()
{
        TITLE="Setup the extend module"

        TEXT[1]="build all extend module"
        FUNC[1]="build_extend_modules"
        
        TEXT[2]="build the ace module"
        FUNC[2]="build_ace"
        
        TEXT[3]="build the osip2 module"
        FUNC[3]="build_osip2"
        
        TEXT[4]="build the exosip module"
        FUNC[4]="build_exosip"
        
        TEXT[5]="build the libevent module"
        FUNC[5]="build_libevent"
        
        TEXT[6]="build the vms module"
        FUNC[6]="build_vms"
}

cc_func()
{
        TITLE="Setup the cc module"

        TEXT[1]="build the svs_access_control module"
        FUNC[1]="build_access_control"
}

mu_func()
{
        TITLE="Setup the mu module"

        TEXT[1]="build the svs_mu_stream module"
        FUNC[1]="build_mu_stream"

        TEXT[2]="build the svs_mu_record module"
        FUNC[2]="build_mu_record"
}

all_func()
{
        TITLE="build all module,CC,MU,EXTEND  "

        TEXT[1]="build all module"
        FUNC[1]="setup"
        
        TEXT[2]="package all module "
        FUNC[2]="package_all"
}

STEPS[1]="all_func"
STEPS[2]="extend_func"
STEPS[3]="cc_func"
STEPS[4]="mu_func"

QUIT=0

while [ "$QUIT" == "0" ]; do
	OPTION_NUM=1

	for s in $(seq ${#STEPS[@]}) ; do
		${STEPS[s]}

		echo "----------------------------------------------------------"
		echo " Step $s: ${TITLE}"
		echo "----------------------------------------------------------"

		for i in $(seq ${#TEXT[@]}) ; do
			echo "[$OPTION_NUM] ${TEXT[i]}"
			OPTIONS[$OPTION_NUM]=${FUNC[i]}
			let "OPTION_NUM+=1"
		done

		# Clear TEXT and FUNC arrays before next step
		unset TEXT
		unset FUNC

		echo ""
	done

	echo "[$OPTION_NUM] Exit Script"
	OPTIONS[$OPTION_NUM]="quit"
	echo ""
	echo -n "Option: "
	read our_entry
	echo ""
	${OPTIONS[our_entry]} ${our_entry}
	echo
done
