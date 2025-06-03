
function sendCommand(cmd) {
  fetch('/' + cmd)
    .then(res => res.text())
    .then(txt => console.log(txt));
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
          fetch('/playfile?name=' + encodeURIComponent(file));
        };
        list.appendChild(li);
      });
    });
}

window.onload = loadPlaylist;
