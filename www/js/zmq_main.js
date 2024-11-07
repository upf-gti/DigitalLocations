const zmq = require('jszmq');
global.window.zmq = zmq;

/* When creating the bundle, add:

if(this.options.onconnect)
    this.options.onconnect();

in WebSocketEndpoint method "onOpen" to get the 
socket opening event.
*/