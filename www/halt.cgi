#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Halt</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Halting...</h3>
<pre>"
gpio -g read 8
echo "</pre>
</body>
</html>"
