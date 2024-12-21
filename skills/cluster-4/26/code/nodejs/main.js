const fs = require('fs');
const path = require('path');
const Datastore = require('nedb');


const db = new Datastore({ filename: path.join(__dirname, 'data', 'sensorData.db'), autoload: true });

// Step 1: Read and split the smoke.txt file
fs.readFile(path.join(__dirname, 'smoke.txt'), 'utf-8', (err, data) => {
    if (err) {
        console.error('Error reading file:', err);
        return;
    }

    // Split the file content by lines and remove the header
    const lines = data.trim().split('\n').slice(1);

    // split each line and prepare data for the insertion
    const entries = lines.map(line => {
        const [time, id, smoke, temp] = line.split(/\s+/);
        return {
            time: parseInt(time, 10),
            sensor_id: parseInt(id, 10),
            smoke: Boolean(parseInt(smoke, 10)),
            temperature: parseFloat(temp)
        };
    });

    // Step 2: Insert data into the database
    db.insert(entries, (err, docs) => {
        if (err) {
            console.error('Error inserting data:', err);
            return;
        }
        console.log('Data successfully inserted into the database.');

        // Step 3: Query the database for sensor_id=1 with smoke
        db.find({ sensor_id: 1, smoke: true }, { time: 1, temperature: 1, _id: 0 }, (err, docs) => {
            if (err) {
                console.error('Error querying database:', err);
            } else {
                console.log('Query results for sensor_id 1 with smoke:');
                console.log(docs);
            }
        });
    });
});
