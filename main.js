var cp = require('child_process');
var bufferTools = require('buffertools');
var fs = require('fs');
var registeredCallback = null;
var child = null;

var mainProcessShutdown = false;
var outstandingRequests = {};
var nextRequestID = 1;

// Check if we've the right permissions (we need to be root
// to access /dev/mem).  Otherwise we'll segfault in the bcm2835init()
// function, at least this will be a bit friendlier
f = fs.openSync('/dev/mem', 'r');
fs.closeSync(f);
// Check done.

// Spawn the process to check for tags
child = cp.fork(__dirname + "/" + "rc522_output.js");

child.on('message', function(msg) {
    if ((msg.topic === 'tagPresented') || (msg.topic === 'tagRemoved')) {
        if(registeredCallback instanceof Function) {
            registeredCallback(msg.topic, msg.payload);
        }
    } else {
        // See if we've got a queued request to match this
        //console.log("Received message: {topic: "+msg.topic+", id: "+msg.id+", firstArg: "+msg.firstArg+", secondArg: "+msg.secondArg+"}");
        // If the second argument is a string, then it's really a hex-encoded buffer.
        // Convert it back
        if (typeof(msg.secondArg) === 'string') {
            var tmpBuf = new Buffer(msg.secondArg);
            msg.secondArg = bufferTools.fromHex(tmpBuf);
        }
        if (outstandingRequests[msg.id] != undefined) {
            //console.log(outstandingRequests[msg.id].topic);
            //console.log(msg.topic);
            if (outstandingRequests[msg.id].topic === msg.topic) {
                // The ID and topic match, so this should be good to reply
                //console.log(outstandingRequests[msg.id].callback);
                //console.log(typeof outstandingRequests[msg.id].callback);
                outstandingRequests[msg.id].callback(msg.firstArg, msg.secondArg);
                //console.log("callback called");
                delete outstandingRequests[msg.id]
            } else {
                console.log("ERROR: topic doesn't match stored request - "+outstandingRequests[msg.id]);
            }
        } else {
            console.log("ERROR: Message received that we didn't have a request for");
        }
    }
});

child.on('close', function(code) {
    if(!mainProcessShutdown)
    {
        // We aren't shutting down, the child process exited for some reason
        // Try kicking it off again
        //init();
        console.log("would've called init");
    }
});

// Internal function to manage comms to the child process
function sendMessage(aTopic, aFirstArg, aSecondArg, aCallback) {
    // Make a note of who asked for this
    outstandingRequests[nextRequestID] = { topic: aTopic, callback: aCallback };
    child.send({ topic: aTopic, id: nextRequestID, firstArg: aFirstArg, secondArg: aSecondArg });
    // We've used this ID now...
    nextRequestID = nextRequestID + 1;
}

//////
// External API
//
function registerTagCallback(aCallback) {
    registeredCallback = aCallback;
}

function readPage(aPage, aCallback) {
    sendMessage("readPage", aPage, undefined, aCallback);
}

async function readPageAsync(aPage) {
    return new Promise(function(resolve, reject) {
        readPage(aPage, function(err, data) {
            if (err != 0) {
                reject(err);
            } else {
                resolve(data);
            }
        });
    });
}

function readFirstNDEFTextRecord(aCallback) {
    sendMessage("readFirstNDEFTextRecord", undefined, undefined, aCallback);
}

async function readFirstNDEFTextRecordAsync() {
    return new Promise(function(resolve, reject) {
        readFirstNDEFTextRecord(function(err, data) {
            if (err != 0) {
                reject(err);
            } else {
                resolve(data);
            }
        });
    });
}

function writePage(aPage, aData, aCallback) {
    sendMessage("writePage", aPage, bufferTools.toHex(aData), aCallback);
}

async function writePageAsync(aPage, aData) {
    return new Promise(function(resolve, reject) {
        writePage(aPage, aData, function(err) {
            if (err != 0) {
                reject(err);
            } else {
                resolve(true);
            }
        });
    });
}

//
// End of external API
//////

// SIGTERM AND SIGINT will trigger the exit event.
process.once("SIGTERM", function () {
    process.exit(0);
});
process.once("SIGINT", function () {
    process.exit(0);
});

// And the exit event shuts down the child.
process.once("exit", function () {
    mainProcessShutdown = true;
    child.kill();
});

// This is a somewhat ugly approach, but it has the advantage of working
// in conjunction with most of what third parties might choose to do with
// uncaughtException listeners, while preserving whatever the exception is.
process.once("uncaughtException", function (error) {
    // If this was the last of the listeners, then shut down the child and rethrow.
    // Our assumption here is that any other code listening for an uncaught
    // exception is going to do the sensible thing and call process.exit().
    console.log(error);
    if (process.listeners("uncaughtException").length === 0) {
        mainProcessShutdown = true;
        child.kill();
        throw error;
    }
});

module.exports = exports = {
    readPage: readPage,
    readPageAsync: readPageAsync,
    registerTagCallback: registerTagCallback,
    writePage: writePage,
    writePageAsync: writePageAsync,
    readFirstNDEFTextRecord: readFirstNDEFTextRecord,
    readFirstNDEFTextRecordAsync: readFirstNDEFTextRecordAsync
}
