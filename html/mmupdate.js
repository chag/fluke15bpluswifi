window.onload=function() {
	var ctx = document.getElementById('mmchart').getContext('2d');
	myData = {
		labels: [1, 2, 3, 4, 5, 6, 7],
		datasets: [
			{
				fillColor: "rgba(151,187,205,0.2)",
				strokeColor: "rgba(151,187,205,1)",
				pointColor: "rgba(151,187,205,1)",
				pointStrokeColor: "#fff",
				data: [28, 48, 40, 19, 86, 27, 90]
			}
		]
	};
	

	var myLiveChart=new Chart(ctx, {
			'type': 'line',
			'data': myData
		});
	
	setInterval(function(data, chart){
		data.labels.push(Math.random()*10);
		data.datasets[0].data.push(Math.random()*100);
		chart.update();
	}.bind(0, myData, myLiveChart), 2000);
}
