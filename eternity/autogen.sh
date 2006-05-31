mkdir autotools

aclocal
automake -a -c
autoconf
automake

./configure
