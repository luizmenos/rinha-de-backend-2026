PHP_ARG_ENABLE([kdtree],
[whether to enable kdtree],
[AS_HELP_STRING([--enable-kdtree], [Enable kdtree extension])],
[no])

if test "$PHP_KDTREE" != "no"; then
  PHP_ADD_INCLUDE([$srcdir/../../lib])

  PHP_NEW_EXTENSION([kdtree], [php_kdtree.c  ../../lib/kdtree.c], [$ext_shared])
fi