UPX_PATH='/opt/local/bin/upx'
UPX_OPTIONS='-9'

if [ -f "$UPX_PATH" -a -x "$UPX_PATH" ];then
    $UPX_PATH $UPX_OPTIONS "$TARGET_BUILD_DIR/$EXECUTABLE_PATH"
else
    echo "WARNING: UPX executable packer not found at $UPX_PATH."
    echo "Eternity file size will not be optimal."
    echo "You can install UPX via a package manager such as MacPorts."
    echo "Download and install MacPorts (or equivalent) and say:"
    echo "'sudo port install upx' (or equivalent)."
fi

FIND_HFILES_RESULT="`find "$TARGET_BUILD_DIR/$WRAPPER_NAME" -name "*.h"`"

if [ ! -z "$FIND_HFILES_RESULT" ];then
    rm -f $FIND_HFILES_RESULT
fi