UPX_PATH='/opt/local/bin/upx'
UPX_OPTIONS='-9'

# call upx on the executable
if [ -f "$UPX_PATH" -a -x "$UPX_PATH" ];then
$UPX_PATH $UPX_OPTIONS "$TARGET_BUILD_DIR/$EXECUTABLE_PATH"
fi

# now find and delete all *.h files
find "$TARGET_BUILD_DIR/$WRAPPER_NAME" -name '*.h' -print0 | xargs -0 rm