use std::collections::VecDeque;
use once_cell::sync::Lazy;
use crate::{config, outputln};

static LOG_CACHE: Lazy<tokio::sync::Mutex<VecDeque<(String, i64, String)>>> = Lazy::new(||
    tokio::sync::Mutex::new(VecDeque::new())
);

static mut LOG_CACHE_LIMIT: Option<usize> = None;
static mut VERBOSE: Option<bool> = None;

pub async fn add_entry(identity: String, timestamp: i64, message: String) {
    let log_cache_limit = unsafe {
        LOG_CACHE_LIMIT.expect("log_queue.rs wasn't initialized")
    };

    let verbose = unsafe {
        VERBOSE.expect("log_queue.rs is not initialized")
    };

    let mut cache_acquired = LOG_CACHE.lock().await;

    // make sure the log never overfills
    if cache_acquired.len() >= log_cache_limit {
        if verbose {
            outputln!("queue", "cache is overflowing, removing one record to add a new one");
        }

        cache_acquired.pop_front();
    }

    cache_acquired.push_back((identity, timestamp, message));
}

async fn worker_thread() {
    let interval_time = {
        std::time::Duration::from_secs(config::get_clone().await.dispatch_sleep_ms)
    };

    let verbose = unsafe {
        VERBOSE.expect("log_queue.rs is not initialized")
    };

    loop {
        tokio::time::sleep(interval_time).await;

        let mut cache_locked = LOG_CACHE.lock().await;
        let mut entries_sent = 0 as usize;

        while !cache_locked.is_empty() {
            // TODO: send via QUIC to the server
            let _ = cache_locked.pop_front();

            if entries_sent.checked_add(1).is_none() {
                if verbose {
                    // if overflow happens (which it won't prolly ever) it will print for now how many
                    // and start over again from 0
                    outputln!("queue", "sent {} entires", entries_sent);
                }

                entries_sent = 0;
            }
        }

        if entries_sent > 0 && verbose {
            outputln!("queue", "sent {} entires", entries_sent);
        }
    }
}

pub async fn initialize() -> anyhow::Result<()> {
    let config_clone = config::get_clone().await;

    unsafe {
        LOG_CACHE_LIMIT = Some(config_clone.maximum_log_entries);
        VERBOSE = Some(config_clone.verbose);
    }

    tokio::spawn(async {
        worker_thread
    });

    Ok(())
}
