<script src="https://cdn.jsdelivr.net/npm/chart.js@3.3.2"></script>
<script src="https://cdn.jsdelivr.net/npm/luxon@1.27.0"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-luxon@1.0.0"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-streaming@2.0.0"></script>
<script>
Chart.register(ChartStreaming);
const bgpl={id:'cbg',beforeDraw:(c)=>{const x=c.canvas.getContext('2d');x.save();x.globalCompositeOperation='destination-over';x.fillStyle='lightgrey';x.fillRect(0,0,c.width,c.height);x.restore();}};
const cfg={type:'line',plugins:[bgpl],data:{datasets:[
{label:'Dach',backgroundColor:'Red',borderColor:'Red',fill:false,data:[]},
{label:'Garage',backgroundColor:'Green',borderColor:'Green',fill:false,data:[]},
{label:'Gartenhaus',backgroundColor:'Cyan',borderColor:'Cyan',fill:false,data:[]},
{label:'Garten',backgroundColor:'Yellow',borderColor:'Yellow',fill:false,data:[]}
]},options:{responsive:false,maintainAspectRatio:false,scales:{x:{type:'realtime',realtime:{delay:2000,duration:60000,frameRate:30,refresh:1000,pause:false,
onRefresh:c=>{fetch('/tc_array').then(r=>r.json()).then(d=>{const a=d.array;var i=0;c.data.datasets.forEach(ds=>{ds.data.push({x:Date.now(),y:a[i++]});});});}
}}},interaction:{intersect:false}}};
cfg.options.plugins={annotation:false,datalabels:false,zoom:false};
new Chart(document.getElementById('schart'),cfg);
</script>
