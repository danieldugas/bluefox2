#!/bin/bash

#----------------------------------------------------------------------------------------------------------
#                                            Argument Parsing
#----------------------------------------------------------------------------------------------------------
SHOW_HELP=NO
PYTHON_VERSION=undefined

echo "-----------------------------------------------------------------------------------------------------------"
echo "                    mvIMPACT Acquire script for swig based Python wrapper generation                       "
echo "-----------------------------------------------------------------------------------------------------------"
echo 'To compile the wrapper code it is necessary to have the python-dev package installed on this system.' 
if [[ $# -gt 0 ]] ; then
    if [ "$1" == "-h" ] || [ "$1" == "--help" ] ; then
        SHOW_HELP=YES
    elif [ "$1" == "python2" ] || [ "$1" == "Python2" ] || [ "$1" == "PYTHON2" ]; then
        PYTHON_VERSION=python
    elif [ "$1" == "python3" ] || [ "$1" == "Python3" ] || [ "$1" == "PYTHON3" ]; then
        PYTHON_VERSION=python3
    fi
fi
#----------------------------------------------------------------------------------------------------------
#                                            Help & Checks
#----------------------------------------------------------------------------------------------------------
if [ "$SHOW_HELP" == "YES" ] ; then
  echo
  echo 'mvIMPACT Acquire script for swig based Python wrapper generation'
  echo
  echo "Usage example(create and install wrapper code):                 ./compileWrapperCode"
  echo "Usage Example(create and install wrapper code using python 2):  ./compileWrapperCode python2"
  echo
  echo "Arguments:"
  echo
  echo "-h --help                       Display this help."
  echo
  echo "python2                         Compile the wrapper code using Python 2"
  echo "python3                         Compile the wrapper code using Python 3"
  echo
  exit 22
fi

if [ "$PYTHON_VERSION" == "undefined" ]; then
    # Determine if the Python 3 compatible version of the Wrapper should be generated 
    echo ""
    echo "Should the mvIMPACT Acquire Python wrapper be available for Python3 (default is 'Python 2')?"
    echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
  read PYTHON3_SUPPORT
  else
    PYTHON3_SUPPORT=""
fi

if [ "$PYTHON_VERSION" == "undefined" ]; then
    if [ "$PYTHON3_SUPPORT" == "n" ] || [ "$PYTHON3_SUPPORT" == "N" ]; then
        PYTHON_VERSION=python
    else
        PYTHON_VERSION=python3
    fi
fi

if [ "$(which $PYTHON_VERSION)x" != "x" ]; then
    export PYTHON_BIN=$(which $PYTHON_VERSION)
else
    echo "Python executable could not be located!!!"
    exit 42
fi

echo "-----------------------------------------------------------------------------------------------------------"
echo "                             Building Python wrapper for $PYTHON_VERSION...                                "
echo "-----------------------------------------------------------------------------------------------------------"
# nice due to linking stage taking forever on x86_64 systems
sudo -E nice -n -8 $PYTHON_BIN setup.py build

echo "-----------------------------------------------------------------------------------------------------------"
echo "                             Installing Python wrapper for $PYTHON_VERSION...                              "
echo "-----------------------------------------------------------------------------------------------------------"
sudo -E $PYTHON_BIN setup.py install
