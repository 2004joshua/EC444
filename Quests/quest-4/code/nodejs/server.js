const express = require('express');
const dgram = require('dgram'); // For UDP communication
const fs = require('fs'); // For file handling
const { Db } = require('tingodb')(); // Import Tingodb for lightweight database

const app = express();
const udpServer = dgram.createSocket('udp4');
const PORT = 3000; // Web server port
const UDP_PORT = 8080; // UDP port for ESP communication

// ======== DATABASE SETUP ========
const db = new Db('./db', {}); // Initialize Tingodb instance

// Ensure the database file exists
if (!fs.existsSync('./db/votes.tingo')) {
    fs.writeFileSync('./db/votes.tingo', ''); // Create empty file if not present
}

// Initialize the votes collection
const votesCollection = db.collection('votes.tingo');

// ======== HELPER FUNCTIONS ========

// Parse the votes collection and return counts
function getVoteCounts(callback) {
    votesCollection.find({}).toArray((err, votes) => {
        if (err) {
            console.error('Failed to fetch votes:', err.message);
            return callback({ Red: 0, Green: 0, Blue: 0 });
        }
        const voteCounts = { Red: 0, Green: 0, Blue: 0 };
        votes.forEach((vote) => {
            if (vote.vote === 'R') voteCounts.Red++;
            else if (vote.vote === 'G') voteCounts.Green++;
            else if (vote.vote === 'B') voteCounts.Blue++;
        });
        callback(voteCounts);
    });
}

// Reset the votes collection
function resetVotes(callback) {
    votesCollection.remove({}, { multi: true }, (err, numRemoved) => {
        if (err) {
            console.error('Failed to reset votes:', err.message);
            callback(false);
        } else {
            console.log(`Database reset. ${numRemoved} entries removed.`);
            callback(true);
        }
    });
}

// ======== ROUTES ========

// Serve static files for the UI
app.use(express.static('src'));

// API to fetch votes for the UI
app.get('/api/votes', (req, res) => {
    getVoteCounts((voteCounts) => {
        res.json(voteCounts);
    });
});

// API to reset the database
app.post('/api/reset', (req, res) => {
    resetVotes((success) => {
        if (success) {
            res.json({ success: true, message: 'Votes have been reset!' });
        } else {
            res.status(500).json({ success: false, message: 'Failed to reset votes.' });
        }
    });
});

// ======== UDP SERVER ========

// Handle UDP messages from ESP coordinator
udpServer.on('message', (msg, rinfo) => {
    const rawMessage = msg.toString().trim();
    console.log(`Raw message received from ${rinfo.address}:${rinfo.port} - ${rawMessage}`);

    let fob_id = '';
    let vote = '';
    let state = 0; // 0 = Looking for ID, 1 = Looking for Vote

    // Loop through the raw message starting after 'S:'
    for (let i = 2; i < rawMessage.length; i++) {
        const char = rawMessage[i];

        if (char === ':') {
            state++;
            if (state === 2) break; // Stop once we process both fields
            continue;
        }

        if (state === 0) {
            fob_id += char; // Accumulate the ID
        } else if (state === 1) {
            vote += char; // Accumulate the vote color
        }
    }

    // Remove any null characters from the vote
    vote = vote.replace(/\0/g, '');

    const timestamp = new Date().toISOString(); // Current timestamp

    // Log polished data
    console.log(`Extracted data: Voter ID: ${fob_id}, Vote: ${vote}`);

    // Insert polished data into the database
    const voteEntry = { fob_id, vote, timestamp }; // Create a JSON object
    votesCollection.insert(voteEntry, (err) => {
        if (err) {
            console.error('Failed to insert vote into database:', err.message);
        } else {
            console.log(`Vote logged to database: Fob ID: ${fob_id}, Vote: ${vote}, Timestamp: ${timestamp}`);
        }
    });
});

// Start listening for UDP messages
udpServer.bind(UDP_PORT, () => {
    console.log(`UDP server listening on port ${UDP_PORT}`);
});

// ======== START SERVERS ========

// Start the Express server for the UI
app.listen(PORT, () => {
    console.log(`Web server running at http://localhost:${PORT}`);
});
