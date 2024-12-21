const socket = io();
const ctx = document.getElementById('espChart').getContext('2d');

// Leader display element
const leaderDisplay = document.getElementById('leader');

// Initialize Chart.js
const chart = new Chart(ctx, {
    type: 'bar',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Resting Time',
                data: [],
                backgroundColor: 'rgba(54, 162, 235, 0.6)',
            },
            {
                label: 'Active Time',
                data: [],
                backgroundColor: 'rgba(75, 192, 192, 0.6)',
            },
            {
                label: 'Highly Active Time',
                data: [],
                backgroundColor: 'rgba(255, 99, 132, 0.6)',
            },
        ],
    },
    options: {
        responsive: true,
        scales: {
            x: { title: { display: true, text: 'Device' } },
            y: { title: { display: true, text: 'Time (s)' } },
        },
    },
});

// Listen for updates from server
socket.on('update', (data) => {
    const { espData } = data;

    const labels = Object.keys(espData);
    const restingTimes = labels.map((device) => espData[device].resting_time);
    const activeTimes = labels.map((device) => espData[device].active_time);
    const highlyActiveTimes = labels.map((device) => espData[device].highly_active_time);

    // Update Chart.js
    chart.data.labels = labels;
    chart.data.datasets[0].data = restingTimes;
    chart.data.datasets[1].data = activeTimes;
    chart.data.datasets[2].data = highlyActiveTimes;
    chart.update();

    // Find the leader based on the highest combined active and highly active time
    let leader = null;
    let maxCombinedTime = 0;

    labels.forEach((device) => {
        const combinedTime = espData[device].active_time + espData[device].highly_active_time;
        if (combinedTime > maxCombinedTime) {
            maxCombinedTime = combinedTime;
            leader = { device_name: device, combinedTime };
        }
    });

    // Update the leader display
    if (leader) {
        leaderDisplay.textContent = `Leader: ${leader.device_name} (Combined Active Time: ${leader.combinedTime}s)`;
    } else {
        leaderDisplay.textContent = 'No leader available.';
    }
});
