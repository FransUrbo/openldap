openldap

This is my fork of the OpenLDAP (http://www.openldap.org/software/repo/openldap.git) GIT repo
with a Debian GNU/Linux directory so that I can easily build my own packages without hassle.

The upstream svn tags I have commited here can be found using:

    git tag -l 'upstream/*'

My own tags I'm using with

   git tag -l 'r*'

The Debian GNU/Linux directory was (originaly) taken from the Debian GNU/Linux Wheezy package
version 2.4.31-1+nmu2.

It was however modified to compile against OpenSSL instead of GnuTLS. Which, by the way, sucks
big hairy balls and should be illegal!
