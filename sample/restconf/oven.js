//const prefix = 'http://127.0.0.1:8080'
const prefix = ''

function fetchStatus() {
	fetch(prefix + '/restconf/ds/ietf-datastores:operational/oven:oven-state')
	.then(function (response) {
		return response.json();
	}).then(function (data) {
		window.temperature.textContent = data['oven:oven-state']['temperature'];
		window.food_inside.textContent = data['oven:oven-state']['food-inside'];
	}).catch(function (err) {
		console.log('error: ' + err);
	});
	fetch(prefix + '/restconf/ds/ietf-datastores:running/oven:oven')
	.then(function (response) {
		return response.json();
	}).then(function (data) {
		window.running.textContent = data['oven:oven']['temperature'];
	}).catch(function (err) {
		console.log('error: ' + err);
	});
}

window.addEventListener('load', function () {
	// Your document is loaded.
	var fetchInterval = 1000;

	setInterval(fetchStatus, fetchInterval);
	fetchStatus();
});

function setTemperature(temperature) {
	console.log(temperature);
	fetch(prefix + '/restconf/ds/ietf-datastores:running/oven:oven', {
		method: "PUT",
		body: JSON.stringify({
			"oven:oven": {
				"temperature": temperature
			}
		})
	}).then(function (response) {
		window.error.textContent = 'temperature set'
	}).catch(function (err) {
		window.error.textContent = 'set Temperature error: ' + err
	});
}

function Insert(time) {
	fetch(prefix + '/restconf/operations/oven:insert-food', {
		method: "POST",
		body: JSON.stringify({
			"oven:input": {
				"time": time
			}
		})
	}).then(function (response) {
		window.error.textContent = 'inserted'
	}).catch(function (err) {
		window.error.textContent = 'Insert error: ' + err
	});
}

function remove() {
	fetch(prefix + '/restconf/operations/oven:remove-food', {
		method: "POST"
	}).then(function (response) {
		window.error.textContent = 'removed'
	}).catch(function (err) {
		window.error.textContent = 'remove error: ' + err
	});
}

function turnonoff(mode) {
	fetch(prefix + '/restconf/ds/ietf-datastores:running/oven:oven', {
		method: "PUT",
		body: JSON.stringify({
			"oven:oven": {
				"turned-on": mode
			}
		})
	}).then(function (response) {
		window.error.textContent = 'turn ' + mode ? "on" : "off"
	}).catch(function (err) {
		window.error.textContent = 'turn on/off error: ' + err
	});
}
