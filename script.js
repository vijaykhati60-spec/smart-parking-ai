
let currentUser = "";
let exitHistory = [];

const PARKING_RATE = 20;

// Connect to Flask API
// Connect to Flask API
async function api(url, method = "GET", data = null) {
    const options = {
        method,
        headers: {
            "Accept": "application/json"
        }
    };

    if (data !== null) {
        options.headers["Content-Type"] = "application/json";
        options.body = JSON.stringify(data);
    }

    const response = await fetch(url, options);

    const responseText = await response.text();

    let result;

    try {
        result = JSON.parse(responseText);
    } catch (error) {
        console.error("Server returned non-JSON:", responseText);

        throw new Error(
            `Server error (${response.status}). Check Flask terminal.`
        );
    }

    if (!response.ok) {
        throw new Error(
            result.error || result.message || "Request failed"
        );
    }

    return result;
}

// Login existing user
async function authenticate() {
    const username = document.getElementById("username").value.trim();
    const password = document.getElementById("password").value;
    const vehicle = document.getElementById("vehicleNumber").value
        .trim().toUpperCase();
    const message = document.getElementById("loginMessage");

    if (!username || !password || !vehicle) {
        message.textContent =
            "Enter username, password and vehicle number.";
        return;
    }

    try {
        const result = await api("/api/login", "POST", {
            username, password
        });

        currentUser = result.user.username;

        document.getElementById("loginPage").classList.add("hidden");
        document.getElementById("dashboard").classList.remove("hidden");
        document.getElementById("welcomeUser").textContent = currentUser;
        document.getElementById("vehicleInput").value = vehicle;

        message.textContent = "";

        await renderDashboard();
        await loadHistory();

        showMessage("Welcome back to ParkEase!");

    } catch (error) {
        message.textContent = error.message;
    }
}


// Register new user
async function registerUser() {
    const username = document.getElementById("username").value.trim();
    const password = document.getElementById("password").value;
    const message = document.getElementById("loginMessage");

    if (!username || !password) {
        message.textContent = "Enter username and password to register.";
        return;
    }

    try {
        const result = await api("/api/register", "POST", {
            username, password
        });

        message.textContent = result.message + " You can now log in.";

    } catch (error) {
        message.textContent = error.message;
    }
}


// Logout
function logout() {
    currentUser = "";

    document.getElementById("dashboard").classList.add("hidden");
    document.getElementById("loginPage").classList.remove("hidden");
    document.getElementById("password").value = "";
    document.getElementById("loginMessage").textContent = "";
}


// Read vehicle number
function readVehicle() {
    const input = document.getElementById("vehicleInput");
    const vehicle = input.value.trim().toUpperCase();

    if (!vehicle) {
        showMessage("Please enter a vehicle number.", true);
        input.focus();
        return null;
    }

    return vehicle;
}


// Park vehicle
async function entry() {
    const vehicle = readVehicle();
    if (!vehicle) return;

    try {
        const result = await api("/api/park", "POST", {
            vehicle_number: vehicle
        });

        showMessage(result.message);

        document.getElementById("vehicleInput").value = "";

        await renderDashboard();

    } catch (error) {
        showMessage(error.message, true);
    }
}


// Exit vehicle and display parking bill
async function exitVehicle() {
    const vehicle = readVehicle();
    if (!vehicle) return;

    try {
        const result = await api("/api/leave", "POST", {
            vehicle_number: vehicle
        });

        // Display the bill
        showBill(result);

        let message = result.message;

        if (result.next_vehicle_parked) {
            message +=
                ` ${result.next_vehicle_parked} moved into Slot ${result.freed_slot}.`;
        }

        showMessage(message);

        document.getElementById("vehicleInput").value = "";

        await renderDashboard();
        await loadHistory();

    } catch (error) {
        showMessage(error.message, true);
    }
}


// Format date and time
function formatDateTime(value) {
    if (!value) return "Not available";
    return String(value);
}


// Format duration
function formatDuration(seconds) {
    seconds = Math.max(0, Number(seconds) || 0);

    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const remainingSeconds = seconds % 60;

    if (hours === 0 && minutes === 0) {
        return `${remainingSeconds} seconds`;
    }

    if (hours === 0) {
        return `${minutes} minute(s)`;
    }

    return `${hours} hour(s) ${minutes} minute(s)`;
}


// Create text safely
function addTextElement(parent, tag, text, className = "") {
    const element = document.createElement(tag);
    element.textContent = text;

    if (className) element.className = className;

    parent.appendChild(element);
    return element;
}


// Display the parking bill
function showBill(bill) {
    let billBox = document.getElementById("parkingBill");

    if (!billBox) {
        billBox = document.createElement("section");
        billBox.id = "parkingBill";
        billBox.className = "parking-bill";

        const dashboard = document.getElementById("dashboard");

        if (dashboard) {
            dashboard.appendChild(billBox);
        } else {
            document.body.appendChild(billBox);
        }
    }

    billBox.innerHTML = "";

    addTextElement(billBox, "h2", "ParkEase Parking Bill");
    addTextElement(
        billBox, "p",
        "Vehicle Number: " + (bill.vehicle_number || "Not available")
    );
    addTextElement(
        billBox, "p",
        "Parking Slot: " + (bill.slot_id ?? "Not available")
    );
    addTextElement(
        billBox, "p",
        "Entry Time: " + formatDateTime(bill.entry_time)
    );
    addTextElement(
        billBox, "p",
        "Exit Time: " + formatDateTime(bill.exit_time)
    );
    addTextElement(
        billBox, "p",
        "Parking Duration: " + formatDuration(bill.duration_seconds)
    );
    addTextElement(
        billBox, "p",
        "Billed Hours: " + (bill.billed_hours ?? 1)
    );
    addTextElement(
        billBox, "p",
        "Rate: ₹" + (bill.rate_per_hour ?? PARKING_RATE) + " per hour"
    );

    const total = addTextElement(
        billBox, "h3",
        "Total Fee: ₹" + Number(bill.fee || 0).toFixed(2),
        "bill-total"
    );

    total.setAttribute("aria-live", "polite");

    addTextElement(
        billBox, "p",
        "Payment status: To be collected"
    );
}


// Load slots and waiting queue from MySQL
async function renderDashboard() {
    try {
        const slots = await api("/api/slots");
        const queue = await api("/api/queue");

        renderSlots(slots);
        renderQueue(queue);

        const occupied = slots.filter(
            slot => slot.status === "Occupied"
        ).length;

        const availableElement = document.getElementById("available");
        const occupiedElement = document.getElementById("occupied");

        if (availableElement) {
            availableElement.textContent = slots.length - occupied;
        }

        if (occupiedElement) {
            occupiedElement.textContent = occupied;
        }

    } catch (error) {
        showMessage("Could not load dashboard: " + error.message, true);
    }
}


// Display car number in each occupied slot
function renderSlots(slots) {
    const container = document.getElementById("slots");
    if (!container) return;

    container.innerHTML = "";

    slots.forEach(slot => {
        const occupied = slot.status === "Occupied";
        const card = document.createElement("div");

        card.className = occupied
            ? "slot-card occupied-slot"
            : "slot-card available-slot";

        addTextElement(card, "h3", "Slot " + slot.slot_id);

        addTextElement(
            card,
            "p",
            occupied
                ? "Car: " + (slot.vehicle_number || "Number unavailable")
                : "No car parked",
            "slot-vehicle"
        );

        addTextElement(card, "span", slot.status, "slot-status");

        container.appendChild(card);
    });
}


// Display waiting queue
function renderQueue(queue) {
    const list = document.getElementById("queueList");
    if (!list) return;

    list.innerHTML = "";

    if (!queue.length) {
        addTextElement(list, "li", "No vehicles waiting.");
        return;
    }

    queue.forEach((item, index) => {
        addTextElement(
            list,
            "li",
            `${index + 1}. ${item.vehicle_number}`
        );
    });
}


// Load previous bills from MySQL
async function loadHistory() {
    const list = document.getElementById("historyList");

    if (!list) return;

    try {
        exitHistory = await api("/api/history");
        renderHistory();

    } catch (error) {
        list.innerHTML = "";
        addTextElement(
            list,
            "li",
            "Could not load bill history: " + error.message
        );
    }
}


// Display previous parking bills
function renderHistory() {
    const list = document.getElementById("historyList");
    if (!list) return;

    list.innerHTML = "";

    if (!exitHistory.length) {
        addTextElement(list, "li", "No parking bills yet.");
        return;
    }

    exitHistory.forEach(item => {
        const row = document.createElement("li");

        const entryTime = formatDateTime(item.entry_time);
        const exitTime = formatDateTime(item.exit_time);
        const fee = Number(item.fee || 0).toFixed(2);

        addTextElement(
            row,
            "strong",
            `${item.vehicle_number} — ₹${fee}`
        );

        addTextElement(
            row,
            "p",
            `Slot ${item.slot_id} | Entry: ${entryTime} | Exit: ${exitTime}`
        );

        list.appendChild(row);
    });
}


// Display messages
function showMessage(message, isError = false) {
    const box = document.getElementById("message");
    if (!box) return;

    box.textContent = message;
    box.className = isError ? "message error" : "message success";
}


// Load dashboard when page opens
document.addEventListener("DOMContentLoaded", () => {
    renderDashboard();
});
