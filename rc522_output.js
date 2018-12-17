var rc522 = require('./build/Release/rfid_rc522');
var bufferTools = require('buffertools');

// Initialise the reader
var gCurrentTag = undefined;

function checkForTag() {
    ret = rc522.checkForTag();
    if ((ret === undefined) && (gCurrentTag !== undefined)) {
        // We have a tag, and there isn't one on the reader
        process.send({ topic: "tagRemoved", payload: gCurrentTag });
        gCurrentTag = undefined;
    } else if ((ret !== undefined) && (gCurrentTag === undefined)) {
        // We don't have a tag, but there's one on the reader
        gCurrentTag = ret;
        process.send({ topic: "tagPresented", payload: gCurrentTag });
    }
}

// Set up our message handling
process.on('message', function(msg, data) {
    // Buffer payloads are passed across using bufferTools.toHex and .fromHex to hex
    // encode all buffer payloads as strings
	//console.log("c.l: {topic: "+msg.topic+", id: "+msg.id+", firstArg: "+msg.firstArg+", secondArg: "+msg.secondArg+"}");
	if (msg.topic === 'readPage') {
		rc522.readPage(msg.firstArg, function(error, retVal) {
        if (error == 0) {
            // Convert the returned data to a hex string
            retVal = bufferTools.toHex(retVal);
        }
		process.send({ topic: "readPage", id: msg.id, firstArg: error, secondArg: retVal}); });
	} else if (msg.topic === 'readFirstNDEFTextRecord') {
        rc522.readFirstNDEFTextRecord(function(error, data) {
            if (error == 0) {
                process.send({ topic: "readFirstNDEFTextRecord", id: msg.id, firstArg: 0, secondArg: bufferTools.toHex(data) });
            } else {
                process.send({ topic: "readFirstNDEFTextRecord", id: msg.id, firstArg: error, secondArg: data });
            }
        });
    } else if (msg.topic === 'writePage') {
        // Second arg will be a hex-encoded string, convert it back to a buffer
        var argBuf = new Buffer(msg.secondArg);
        var data = bufferTools.fromHex(argBuf);
		rc522.writePage(msg.firstArg, data, function(retVal) {
		    process.send({ topic: "writePage", id: msg.id, firstArg: retVal, secondArg: undefined}); 
        });
    }
});

// Now start monitoring 
setInterval(checkForTag, 20);

