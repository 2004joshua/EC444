const connect = require("connect");
const CORS = require("connect-cors");
const serveStatic = require("serve-static");

connect()
  .use(CORS())
  .use(serveStatic("public"))
  .listen(8888, () => console.log("Server running on 8888..."));
