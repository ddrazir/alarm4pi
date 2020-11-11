#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Photos</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Images captured:
<ul>"
# To get the alarm4pi directory we can use one of the following lines
# ls `dirname "${BASH_SOURCE[0]}"`/../captures |
# ls "$OLDPWD"/../captures |
ls `dirname "$0"`/../captures |
while IFS= read -r line
do
echo "<li><a href=\"capture.cgi?$line\">$line</a></li>"
done
echo "</ul>
</h3>
</body>
</html>"
