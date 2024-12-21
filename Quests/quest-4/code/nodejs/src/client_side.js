document.addEventListener('DOMContentLoaded', () => {
    const voteList = document.getElementById('vote-list');
    const voteLog = document.getElementById('vote-log');
    const resetButton = document.getElementById('reset-btn');

    // Fetch summarized vote counts
    async function fetchVoteCounts() {
        try {
            const response = await fetch('/api/votes');
            const data = await response.json();
            updateVoteCounts(data);
        } catch (error) {
            console.error('Error fetching vote counts:', error);
        }
    }

    // Fetch full vote log
    async function fetchVoteLog() {
        try {
            const response = await fetch('/api/vote-log');
            const data = await response.json();
            updateVoteLog(data);
        } catch (error) {
            console.error('Error fetching vote log:', error);
        }
    }

    // Update the summarized vote counts
    function updateVoteCounts(voteCounts) {
        voteList.innerHTML = ''; // Clear existing list
        Object.entries(voteCounts).forEach(([color, count]) => {
            const listItem = document.createElement('li');
            listItem.textContent = `${color}: ${count}`;
            voteList.appendChild(listItem);
        });
    }

    // Update the full vote log
    function updateVoteLog(voteLogData) {
        voteLog.innerHTML = ''; // Clear existing log
        voteLogData.forEach(({ fob_id, vote, timestamp }) => {
            const listItem = document.createElement('li');
            listItem.textContent = `Voter ID: ${fob_id}, Vote: ${vote}, Time: ${timestamp}`;
            voteLog.appendChild(listItem);
        });
    }

    // Reset votes
    resetButton.addEventListener('click', async () => {
        try {
            const response = await fetch('/api/reset', { method: 'POST' });
            const result = await response.json();
            if (result.success) {
                voteList.innerHTML = ''; // Clear the UI
                voteLog.innerHTML = ''; // Clear the vote log
                alert(result.message);
            }
        } catch (error) {
            console.error('Error resetting votes:', error);
        }
    });

    // Fetch votes periodically
    fetchVoteCounts();
    fetchVoteLog();
    setInterval(() => {
        fetchVoteCounts();
        fetchVoteLog();
    }, 5000); // Update every 5 seconds
});
