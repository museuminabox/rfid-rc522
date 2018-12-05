var rc522 = require('./build/Release/rfid_rc522');

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
	console.log("c.l: {topic: "+msg.topic+", id: "+msg.id+", payload: "+msg.payload+"}");
	if (msg.topic === 'readPage') {
		rc522.readPage(1, function(retVal) {
		console.log("c-addon ret: "+retVal);
		process.send({ topic: "readPage", id: msg.id, payload: "AMc"}); });
	}
});

// Now start monitoring 
setInterval(checkForTag, 20);



//info = "asdf";
//process.send('pageRead');

//rc522(function(rfidTagSerialNumber) {
	//console.log(rfidTagSerialNumber);
	//process.send({ topic: "tagPresent", payload: rfidTagSerialNumber});
//});

