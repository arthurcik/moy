import http.client
import base64
import logging
import re
import time
import threading
import queue
import socket
import emoji

from telegram.ext import Updater, MessageHandler, Filters

# --- CONFIGURARE ---
BOT_TOKEN = "token"
ALLOWED_CHAT_ID = -100channel
SOAP_USER = "user"              # Cont de GM cu permisiuni RBAC
SOAP_PASS = "pass"
SOAP_IP = "host"
SOAP_PORT = port
# -------------------

#logging.basicConfig(level=logging.INFO)
logging.basicConfig(level=logging.WARNING)

msg_queue = queue.Queue()

# Variabile de stare globale
last_status_check = 0
is_wow_online = True

def is_server_online():
    """Verifica daca portul SOAP este deschis"""
    try:
        with socket.create_connection((SOAP_IP, SOAP_PORT), timeout=1):
            return True
    except:
        return False

def send_to_wow(message_text):
    clean_text = re.sub(r'[<&]', '', message_text)
    soap_payload = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="urn:TC">'
            '<SOAP-ENV:Body>'
                '<ns1:executeCommand>'
                    f'<command>telegram send {clean_text}</command>'
                '</ns1:executeCommand>'
            '</SOAP-ENV:Body>'
        '</SOAP-ENV:Envelope>'
    )
    auth_str = f"{SOAP_USER}:{SOAP_PASS}"
    encoded_auth = base64.b64encode(auth_str.encode('utf-8')).decode('utf-8')

    try:
        conn = http.client.HTTPConnection(SOAP_IP, SOAP_PORT, timeout=5)
        headers = {
            'Content-Type': 'text/xml; charset=utf-8',
            'Authorization': f'Basic {encoded_auth}',
            'Connection': 'close'
        }
        conn.request("POST", "/", body=soap_payload.encode('utf-8'), headers=headers)
        response = conn.getresponse()
        response.read()
        conn.close()
    except Exception as e:
        logging.error(f"Eroare conexiune SOAP: {e}")

def soap_worker():
    """Thread care proceseaza coada asincron cu verificare de port"""
    global last_status_check, is_wow_online
    
    while True:
        message = msg_queue.get()
        if message is None: break
        
        current_time = time.time()
        
        # Verificam portul o data la 10 secunde
        if current_time - last_status_check > 10:
            is_wow_online = is_server_online()
            last_status_check = current_time
            if not is_wow_online:
                logging.warning("Server WoW detectat ca fiind OFFLINE.")
        
        if is_wow_online:
            send_to_wow(message)
            time.sleep(0.2)
        else:
            # Gole?te mesajele vechi cat timp e offline (op?ional)
            pass
            
        msg_queue.task_done()

# Pornim worker-ul o singura data
threading.Thread(target=soap_worker, daemon=True).start()

def handle_group_message(update, context):
    if update.effective_chat.id != ALLOWED_CHAT_ID:
        return

    user_name = update.message.from_user.first_name if update.message.from_user else "Sistem"
    msg_to_send = ""

    # 1. MESAJ TEXT (Include prelucrare Emoji)
    if update.message.text:
        raw_text = update.message.text
        has_emojis = emoji.emoji_count(raw_text) > 0
        # Eliminam emoji-urile pentru a evita '?' in WoW
        clean_text = emoji.replace_emoji(raw_text, replace='')
        clean_text = clean_text.replace('\n', ' ').strip()
        
        if has_emojis:
            msg_to_send = f"{user_name}: {clean_text} [Emoji]" if clean_text else f"{user_name}: [Emoji]"
        else:
            msg_to_send = f"{user_name}: {clean_text}"

    # 2. STICKER
    elif update.message.sticker:
        msg_to_send = f"{user_name}: [Sticker]"

    # 3. POZA
    elif update.message.photo:
        msg_to_send = f"{user_name}: [Imagine]"

    # Trimitere in coada (daca mesajul nu e gol)
    if msg_to_send and len(msg_to_send) > len(user_name) + 2:
        if len(msg_to_send) > 220:
            msg_to_send = msg_to_send[:217] + "..."
        msg_queue.put(msg_to_send)


def main():
    updater = Updater(BOT_TOKEN, use_context=True)
    dp = updater.dispatcher
    
    # Filtru selectiv: DOAR Text, Stickere sau Fotografii
    # Orice altceva (GIF, Audio, Poll, Video) va fi ignorat automat de bot
    selective_filter = (Filters.text | Filters.sticker | Filters.photo) & ~Filters.command
    
    dp.add_handler(MessageHandler(selective_filter, handle_group_message))
    
    logging.info(f"Bridge activat (Mod Selectiv) pentru Chat ID: {ALLOWED_CHAT_ID}")
    updater.start_polling()
    updater.idle()

if __name__ == '__main__':
    main()