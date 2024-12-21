const dataPoints = [];

function createSocket() {
  const socket = io("http://localhost:3000");
  socket.on("connect", () => {
    console.log("Connected to the server");
  });

  socket.on("data", (newData) => {
    dataPoints.push(newData);

    // Keep only the last 30 data points
    if (dataPoints.length > 30) {
      dataPoints.shift();
    }

    updateActivity();
    createCharts();
  });
}

function updateActivity() {
  // If X and Y acceleration are both greater than 5, the cat is on the move
  const lastData = dataPoints[dataPoints.length - 1];
  const xAccel = Math.abs(lastData.xAccel);
  const yAccel = Math.abs(lastData.yAccel);
  const isMoving = xAccel > 5 || yAccel > 5;

  // If the pitch is between 150 and 210, the cat is upside down
  const pitch = Math.abs(lastData.pitch);
  const roll = Math.abs(lastData.roll);
  const isUpsideDown = (pitch > 150 && pitch < 210) || (roll > 150 && roll < 210);
  
  const propercase = (str) => str[0].toUpperCase() + str.slice(1);

  let activity = `${propercase(lastData.name.toLowerCase())} is `;

  if (isMoving) {
    activity += "moving";
  } else if (isUpsideDown) {
    activity += "upside down";
  } else {
    activity += "resting";
  }

  activity += ` (${lastData.time_in_activity}s)`;

  const activityElement = document.getElementById("activity");
  activityElement.innerText = activity;
}

function createCharts() {
  const activityChart = new CanvasJS.Chart("activity-chart", {
    animationEnabled: false,
    title: {
      text: "Cat Tracker",
    },
    axisX: {
      title: "Time",
      suffix: "s",
    },
    axisY: {
      title: "Acceleration",
      suffix: "m/s^2",
    },
    data: [
      {
        type: "line",
        name: "X Acceleration",
        showInLegend: true,
        dataPoints: dataPoints.map((point) => ({
          x: point.seconds,
          y: point.xAccel,
        })),
      },
      {
        type: "line",
        name: "Y Acceleration",
        showInLegend: true,
        dataPoints: dataPoints.map((point) => ({
          x: point.seconds,
          y: point.yAccel,
        })),
      },
      {
        type: "line",
        name: "Z Acceleration",
        showInLegend: true,
        dataPoints: dataPoints.map((point) => ({
          x: point.seconds,
          y: point.zAccel,
        })),
      },
    ],
  });

  const pitchRollChart = new CanvasJS.Chart("pitch-roll-chart", {
    animationEnabled: false,
    title: {
      text: "Pitch and Roll",
    },
    axisX: {
      title: "Time",
      suffix: "s",
    },
    axisY: {
      title: "Angle",
      suffix: "°",
    },
    data: [
      {
        type: "line",
        name: "Pitch",
        showInLegend: true,
        dataPoints: dataPoints.map((point) => ({
          x: point.seconds,
          y: point.pitch,
        })),
      },
      {
        type: "line",
        name: "Roll",
        showInLegend: true,
        dataPoints: dataPoints.map((point) => ({
          x: point.seconds,
          y: point.roll,
        })),
      },
    ],
  });

  const tempChart = new CanvasJS.Chart("temperature-chart", {
    animationEnabled: false,
    title: {
      text: "Temperature",
    },
    axisX: {
      title: "Time",
      suffix: "s",
    },
    axisY: {
      title: "Temperature",
      suffix: "°C",
    },
    data: [
      {
        type: "line",
        name: "Temperature",
        showInLegend: true,
        dataPoints: dataPoints.map((point) => ({
          x: point.seconds,
          y: point.temp,
        })),
      },
    ],
  });

  activityChart.render();
  tempChart.render();
  pitchRollChart.render();
}

window.onload = function () {
  createSocket();
};
