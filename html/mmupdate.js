function MmChart(chartDivId, dispDivId, wsUrl) {
	this.addPoint=function(evt) {
		var obj=JSON.parse(evt.data);
		var mmtxt=obj.value+" "+obj.ml+obj.unit;
		while(this.mmDisp.childNodes.length >= 1) {
			this.mmDisp.removeChild(this.mmDisp.firstChild);
		}
		this.mmDisp.appendChild(this.mmDisp.ownerDocument.createTextNode(mmtxt));
		
		if (this.curUnit!=obj.unit) {
			//Changed units. Kill graph.
			this.chartData.labels.length=0;
			this.chartData.datasets[0].data.length=0;
			this.chartIntervalCt=0;
			this.chartInterval=1;
			this.chartData.datasets[0].label=obj.ml+obj.unit;
			this.curUnit=obj.unit;
			this.curMl=obj.ml;
		}
		
		if (this.curMl!=obj.ml) {
			var mls="fpnum KMGT";
			var oldMl=mls.indexOf(this.curMl);
			var newMl=mls.indexOf(obj.ml);
			if (this.curMl=="") oldMl=5;
			if (obj.ml=="") oldMl=5;
			if (oldMl>newMl) {
				//measurement is smaller, just scale
				obj.value=obj.value/(1000*(oldMl-newMl));
			} else {
				//scale graph so new measurement fits
				var div=(1000*(newMl-oldMl));
				for (var i=0; i<this.chartData.datasets[0].data.length; i++) {
					this.chartData.datasets[0].data[i]/=div;
				}
				this.chartData.datasets[0].label=obj.ml+obj.unit;
				this.curMl=obj.ml;
			}
		}
		
		this.chartIntervalCt++;
		if (this.chartIntervalCt>=this.chartInterval) {
			this.chartData.datasets[0].data.push(obj.value);
			if ((this.chartData.datasets[0].data.length%5)==0) {
				var sec=Math.round((Date.now()-this.chartStart)/1000);
				var min=Math.floor(sec/60);
				sec=sec%60;
				if (sec<10) sec="0"+sec;
				this.chartData.labels.push(min+":"+sec);
			} else {
				this.chartData.labels.push("");
			}
			if (this.chartData.labels.length==this.chartMaxPt) {
				//Reached maximum amount of points. Kill half the points and re-duce interval time.
				var i;
				for (i=this.chartData.labels.length-2; i>=0; i=i-2) {
					this.chartData.labels.splice(i, 1);
					this.chartData.datasets[0].data.splice(i, 1);
				}
				this.chartInterval*=2;
			}
			this.chartIntervalCt=0;
			this.chart.update();
		}
	}

	this.mmDisp=document.getElementById('mmdisplay');
	this.ws=new WebSocket(wsUrl);
	this.ws.onmessage=this.addPoint.bind(this);
	this.chartData={
		labels: [],
		datasets: [
			{
				fillColor: "rgba(255,0,0,0.2)",
				strokeColor: "rgba(255,0,0,1)",
				pointColor: "rgba(255,0,0,1)",
				pointStrokeColor: "rgba(255, 0, 0, 0)",
				data: []
			}
		]
	}
	this.chart=new Chart(document.getElementById(chartDivId).getContext('2d'),
			{'type': 'line', 'data': this.chartData});
	this.chartInterval=1;
	this.chartIntervalCt=0;
	this.chartStart=Date.now();
	this.chartMaxPt=200;
	this.curUnit="";
	this.curMl="";
	this.addBtn=function(el, cmd) {
		el.onclick=function(cmd) {
			this.ws.send(cmd);
		}.bind(this, cmd);
	}
}



window.onload=function() {
	var myMm=new MmChart("mmchart", "mmdisp", "ws://"+window.location.host+"/mmws.cgi");
	myMm.addBtn(document.getElementById('HzBtn'), "hz");
}
