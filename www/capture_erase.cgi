#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Erase photos</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Erasing "
# To get the alarm4pi directory we can use one of the following lines
# ls `dirname "${BASH_SOURCE[0]}"`/../captures |
# ls "$OLDPWD"/../captures |
captures_dir="$(dirname -- "$0")"/../captures
ls "$captures_dir" | wc -l
echo " images captured.</h3>"
rm "$captures_dir"/*
echo "</body>
</html>"
