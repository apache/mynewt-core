# console
#
# There are two versions of this library;
#  full - contains actual implemetation
#  stub - has stubs for the API
#
# You can write of a library which uses console_printf(), and builder of a
# project can select which one they'll use.
# For the library/egg, list in the pkg.yml console as the required capability.
# Project builder will then include either libs/console/full or
# libs/console/stub as their choice.
#
