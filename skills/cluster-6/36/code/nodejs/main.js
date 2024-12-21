const fs = require('fs');
const path = require('path');
const tingodb = require('tingodb')();

// Step 1: Set up TingoDB database
const dbPath = path.join(__dirname, 'data'); // Directory for database files
const db = new tingodb.Db(dbPath, {}); // Initialize TingoDB
const collection = db.collection('sensorData.db'); // Define the collection

// Step 2: Read and split the smoke.txt file
fs.readFile(path.join(__dirname, 'smoke.txt'), 'utf-8', (err, data) => {
    if (err) {
        console.error('Error reading file:', err);
        return;
    }

    // Split the file content by lines and remove the header
    const lines = data.trim().split('\n').slice(1);

    // Prepare data entries for insertion
    const entries = lines.map(line => {
        const [time, id, smoke, temp] = line.split(/\s+/);
        return {
            time: parseInt(time, 10),
            sensor_id: parseInt(id, 10),
            smoke: Boolean(parseInt(smoke, 10)),
            temperature: parseFloat(temp)
        };
    });

    // Step 3: Insert data into the TingoDB collection
    collection.insert(entries, (err) => {
        if (err) {
            console.error('Error inserting data:', err);
            return;
        }
        console.log('Data successfully inserted into the database.');

        // Step 4: Export all data to a JSON file
        collection.find({}).toArray((err, docs) => {
            if (err) {
                console.error('Error querying database:', err);
                return;
            }

            const outputPath = path.join(__dirname, 'data', 'exported_data.json'); // Output JSON file
            try {
                fs.writeFileSync(outputPath, JSON.stringify(docs, null, 2));
                console.log(`Data successfully exported to ${outputPath}`);
            } catch (writeErr) {
                console.error('Error writing to JSON file:', writeErr);
            }
        });
    });
});
