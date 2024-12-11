import _thread
import time
import gc

user_cbs = []

thread_started = False

def main():
    global thread_started, user_cbs
    while True:
        # print("SUP CB")
        try:
            for cb in user_cbs:
                cb()
            time.sleep_ms(50)
        except KeyboardInterrupt:
            # print("keyboard")
            thread_started = False
            user_cbs = []
            return
        except Exception as e:
            print(str(e))
            

def start_thread():
    global thread_started, user_cbs
    # print("starting thread")
    gc.collect()
    thread_started = True
    _thread.start_new_thread(main, ())

def set_background_user_callback(cb):
    global thread_started, user_cbs
    # print("user callback", thread_started)
    user_cbs.append(cb)
    if not thread_started:
        start_thread()

def add_background_user_callback(cb):
    set_background_user_callback(cb)