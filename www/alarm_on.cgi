#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Alarm on</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Enabling alarm (setting to 0)...</h3>
<pre>"
gpio -g write 18 0
echo "</pre>
</body>
</html>"
