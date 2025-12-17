const API_BASE = "http://localhost:8080";

async function apiPost(endpoint, data) {
    const resp = await fetch(API_BASE + endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(data)
    });

    if (!resp.ok) {
        const text = await resp.text();
        throw new Error(text || "Server error");
    }

    return resp.text();   // либо token, либо nick
}
