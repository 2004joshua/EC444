function createSocket() {
  const socket = io("http://localhost:3000");
  socket.on("connect", () => {
    console.log("Connected to the server");
  });

  socket.on("data", (newData) => {
    const stocks = organiseData(newData);
    createChart(stocks);
  });
}

function organiseData(data) {
  const stocks = {};

  for (const entry of data) {
    if (Object.keys(stocks).indexOf(entry.Stock) === -1) {
      stocks[entry.Stock] = [];
    }

    stocks[entry.Stock].push({
      x: Number(entry.Date),
      y: Number(entry.Closing),
    });
  }

  return stocks;
}

function createChart(stocks) {
  const chart = new CanvasJS.Chart("app", {
    animationEnabled: true,
    title: {
      text: "Stock Prices",
    },
    axisX: {
      title: "Day",
    },
    axisY: {
      title: "Price (in USD)",
      prefix: "$",
    },
    data: Object.keys(stocks).map((stock) => ({
      type: "spline",
      name: stock,
      showInLegend: true,
      dataPoints: stocks[stock],
    })),
  });

  chart.render();
}

window.onload = function () {
  createSocket();
};
