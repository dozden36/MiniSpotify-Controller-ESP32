import http.server
import socketserver
import urllib.parse
import webbrowser
import requests
import base64

CLIENT_ID = "YOUR_SPOTIFY_CLIENT_ID"
CLIENT_SECRET = "YOUR_SPOTIFY_CLIENT_SECRET"
REDIRECT_URI = "http://127.0.0.1:8888/callback"
PORT = 8888

SCOPE = "user-read-currently-playing user-read-playback-state user-modify-playback-state"

auth_code = ""

class OAuthHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        global auth_code
        query = urllib.parse.urlparse(self.path).query
        params = urllib.parse.parse_qs(query)
        
        if "code" in params:
            auth_code = params["code"][0]
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write(b"<h1>Authorization successful! You can close this window now.</h1>")
        else:
            self.send_response(400)
            self.end_headers()

def get_refresh_token():
    auth_url = (
        f"https://accounts.spotify.com/authorize?"
        f"client_id={CLIENT_ID}&response_type=code&redirect_uri={urllib.parse.quote(REDIRECT_URI)}"
        f"&scope={urllib.parse.quote(SCOPE)}"
    )
    
    print("Opening browser for Spotify authorization...")
    webbrowser.open(auth_url)

    with socketserver.TCPServer(("", PORT), OAuthHandler) as httpd:
        httpd.handle_request()

    if not auth_code:
        print("Failed to get authorization code.")
        return

    auth_header = base64.b64encode(f"{CLIENT_ID}:{CLIENT_SECRET}".encode()).decode()
    headers = {
        "Authorization": f"Basic {auth_header}",
        "Content-Type": "application/x-www-form-urlencoded"
    }
    payload = {
        "grant_type": "authorization_code",
        "code": auth_code,
        "redirect_uri": REDIRECT_URI
    }

    response = requests.post("https://accounts.spotify.com/api/token", headers=headers, data=payload)
    
    if response.status_code == 200:
        tokens = response.json()
        print("\n=== SUCCESS ===")
        print(f"REFRESH TOKEN: {tokens.get('refresh_token')}")
        print("===============\n")
    else:
        print(f"Error fetching tokens: {response.text}")

if __name__ == "__main__":
    get_refresh_token()
