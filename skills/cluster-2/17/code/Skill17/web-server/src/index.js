const connect = require("connect");
const serveStatic = require("serve-static");

// Serve files from the 'public' directory
connect()
  .use(serveStatic("public"))
  .listen(8080, () => console.log("Static server running on http://localhost:8080"));
