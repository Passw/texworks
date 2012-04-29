#!/bin/sh

PATHFILE=./src/DefaultBinaryPaths.h

FOUNDTEX=0
BINPATHS=":"

if [ -x /usr/share/libtool/config.guess ]; then
	PLATFORM=`/usr/share/libtool/config.guess`
	ARCH=`echo $PLATFORM | sed 's/-.*//;s/i.86/i386/'`
# At least on Debian, Ubuntu, Gentoo and LFS, config.guess seems to be in a
# subdirectory
elif [ -x /usr/share/libtool/config/config.guess ]; then
	PLATFORM=`/usr/share/libtool/config/config.guess`
	ARCH=`echo $PLATFORM | sed 's/-.*//;s/i.86/i386/'`
else
	PLATFORM=`uname -s | tr A-Z a-z`
	ARCH=`uname -m | tr A-Z a-z | sed 's/i.86/i386/'`
fi

# append a path to $BINPATHS unless already present
appendPath()
{
	NEWPATH="$1"
	# Don't append common system directories here (they are added elsewhere)
	# to ensure that they are always at the end of the list
	if [ "$NEWPATH" = "/usr/bin" -o "$NEWPATH" = "/usr/local/bin" ]; then
		echo "$NEWPATH ignored"
		return
	fi
	case "$BINPATHS" in
		*:"$NEWPATH":*)	echo $NEWPATH already present;;
		*)				# note that BINPATHS already ends with colon
						BINPATHS="$BINPATHS$NEWPATH:";;
	esac
}

# (0) for Mac OS X, start with /usr/texbin
case $PLATFORM in
	*darwin*)	appendPath "/usr/texbin";;
esac

# (1) try to find tex and ghostscript

TEX=`which tex`
if [ "$TEX" != "" ]; then
	if [ -x "$TEX" ]; then
		appendPath `dirname "$TEX"`
		FOUNDTEX=1
	fi
fi

GS=`which gs`
if [ "$GS" != "" ]; then
	if [ -x "$GS" ]; then
		appendPath `dirname "$GS"`
	fi
fi

# (2) try to guess default TL path for the current system
# (no idea how much of this actually works....)

case $PLATFORM in
	*aix*)		OS=aix;;
	*cygwin*)	OS=cygwin;;
	*darwin*)	OS=darwin;;
	*freebsd*)	OS=freebsd;;
	*hpux*)		OS=hpux;;
	*irix*)		OS=irix;;
	*linux*)	OS=linux;;
	*netbsd*)	OS=netbsd;;
	*openbsd*)	OS=openbsd;;
	*solaris*)	OS=solaris;;
	*)			OS=`echo $PLATFORM | sed 's/.*-//'`
esac

appendPath "/usr/local/texlive/2012/bin/$ARCH-$OS"
appendPath "/usr/local/texlive/2011/bin/$ARCH-$OS"
appendPath "/usr/local/texlive/2010/bin/$ARCH-$OS"
appendPath "/usr/local/texlive/2009/bin/$ARCH-$OS"
appendPath "/usr/local/texlive/2008/bin/$ARCH-$OS"
appendPath "/usr/local/texlive/2007/bin/$ARCH-$OS"

for TEXLIVEROOT in /usr/local/texlive/* /opt/texlive/*; do
	# Check if this is really a folder (e.g., /opt/... might not exist)
	if [ -d "$TEXLIVEROOT/bin/$ARCH-$OS" ]; then
		# Check that this is of the form /texlive/1234
		if [ -z `basename $TEXLIVEROOT | sed 's/[0-9]//g'` ]; then
			# Paranoia: Make sure there actually is a bin/... subdirectory
			if [ -d "$TEXLIVEROOT/bin/$ARCH-$OS" ]; then
				appendPath "$TEXLIVEROOT/bin/$ARCH-$OS"
			fi
		fi
	fi
done

# (3) append default paths that we should always check

BINPATHS="$BINPATHS/usr/local/bin:/usr/bin"

# strip leading and trailing colons from the list
BINPATHS=`echo $BINPATHS | sed 's/^://;s/:$//'`

echo "// Default paths to TeX binaries, for TeXworks" > $PATHFILE
echo "// Generated by $0" >> $PATHFILE
echo "#define DEFAULT_BIN_PATHS \"$BINPATHS\"" >> $PATHFILE

echo "configuring default paths for TeX binaries as:"
echo "$BINPATHS"

exit $FOUNDTEX
