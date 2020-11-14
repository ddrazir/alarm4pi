#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Processes</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>ps:</h3>
<pre><xmp>"
ps -u root --deselect -o 'pid,user,stat,command'
echo "</xmp></pre>
</body>
</html>"
