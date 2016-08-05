# --------------- esphttpd config options ---------------

# If GZIP_COMPRESSION is set to "yes" then the static css, js, and html files will be compressed
# with gzip before added to the espfs image and will be served with gzip Content-Encoding header.
# This could speed up the downloading of these files, but might break compatibility with older
# web browsers not supporting gzip encoding because Accept-Encoding is simply ignored.
# Enable this option if you have large static files to serve (for e.g. JQuery, Twitter bootstrap)
# By default only js, css and html files are compressed using heatshrink.
# If you have text based static files with different extensions what you want to serve compressed
# then you will need to add the extension to the following places:
# - Add the extension to this Makefile at the webpages.espfs target to the find command
# - Add the extension to the gzippedFileTypes array in the user/httpd.c file
#
# Adding JPG or PNG files (and any other compressed formats) is not recommended, because GZIP
# compression does not work effectively on compressed files.

#Static gzipping is disabled by default.
GZIP_COMPRESSION ?= yes

# If COMPRESS_W_HTMLCOMPRESSOR is set to "yes" then the static css and js files will be compressed with
# htmlcompressor and yui-compressor. This option works only when GZIP_COMPRESSION is set to "yes".
# https://code.google.com/p/htmlcompressor/#For_Non-Java_Projects
# http://yui.github.io/yuicompressor/
# enabled by default.
COMPRESS_W_HTMLCOMPRESSOR ?= yes
HTML_COMPRESSOR ?= htmlcompressor-1.5.3.jar
YUI_COMPRESSOR ?= yuicompressor-2.4.8.jar

# If USE_HEATSHRINK is set to "yes" then the espfs files will be compressed with Heatshrink and
# decompressed on the fly while reading the file.
# Because the decompression is done in the esp8266, it does not require any support in the browser.
USE_HEATSHRINK ?= no
