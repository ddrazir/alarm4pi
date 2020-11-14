#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Log</title>
<meta name="viewport" content="initial-scale=1"></head>
<body>
<h3>alarm4pi daemon log:</h3>
<pre><xmp>"
# cat "$OLDPWD"/../log/daemon.log
cat `dirname "$0"`/../log/daemon.log
echo "</xmp></pre>
</body>
</html>"
