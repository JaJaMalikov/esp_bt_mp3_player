
function sendCommand(cmd) {
  fetch('/' + cmd)
    .then(res => res.text())
    .then(txt => console.log(txt));
}

function updateCurrent() {
  fetch('/current')
    .then(res => res.json())
    .then(data => {
      document.getElementById('current').textContent = data.track;
    });
}

function loadPlaylist() {
  fetch('/list')
    .then(res => res.json())
    .then(files => {
      const list = document.getElementById('playlist');
      list.innerHTML = '';
      files.forEach(file => {
        const li = document.createElement('li');
        li.textContent = file;
        li.onclick = () => {
          fetch('/play?file=' + encodeURIComponent(file));
        };
      list.appendChild(li);
    });
  });
}
window.onload = () => {
  loadPlaylist();
  updateCurrent();
  setInterval(updateCurrent, 5000);
};
