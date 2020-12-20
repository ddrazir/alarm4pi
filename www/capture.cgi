#!/bin/bash
if [[ $QUERY_STRING == *" "* ]] || [[ $QUERY_STRING == *"/"* ]] || [[ $QUERY_STRING == *".."* ]] ; then
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store

<!DOCTYPE html><html><head>
<title>Capture</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Error: The query string in the URL ("
echo "$QUERY_STRING"
echo ") contains invalid characters. Only a filename is expected.</h3>
</body>
</html>"
else
echo "HTTP/1.0 200 OK
Content-type: image/jpg
Connection: close
Cache-Control: no-store
"
# cat "$OLDPWD"/../captures/"$QUERY_STRING"
cat "$(dirname -- "$0")"/../captures/"$QUERY_STRING"
fi
