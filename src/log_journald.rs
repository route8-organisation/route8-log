use anyhow::{anyhow, Context};
use chrono::TimeZone;
use once_cell::sync::Lazy;
use serde_json::json;

use crate::{config, errorln, log_queue, outputln};

static RUNNING_CHECK: Lazy<tokio::sync::Mutex<bool>> = Lazy::new(||
    tokio::sync::Mutex::new(false)
);

static mut VERBOSE: Option<bool> = None;

// journald crate is not tokio aware
fn worker_thread(tokio_runtime: tokio::runtime::Handle, identity: String) -> anyhow::Result<()> {
    outputln!("journald", "starting journald log watcher with identity '{}'", identity);

    let verbose = unsafe {
        VERBOSE.expect("journald is not initialized")
    };
    
    let journald_config = journald::reader::JournalReaderConfig::default();
    let mut journald_reader = journald::reader::JournalReader::open(&journald_config).map_err(|e| anyhow!("failed to open the reader due to {}", e.to_string()))?;

    journald_reader.seek(journald::reader::JournalSeek::Tail).map_err(|e| anyhow!("faild to seek to the end due to {}", e.to_string()))?;

    // hack: moves one less than the tail to get the last entry
    let _ = journald_reader.previous_entry()?.context("failed to get a log entry")?;

    loop {
        let _ = journald_reader.wait().map_err(|e| anyhow!("failed to wait for new event due to {}", e.to_string()));

        // optimization: don't spawn a runtime for every record, wait till the end
        let mut temp_cache: Vec<(i64, String, String)> = Vec::new();

        while let Some(entry) = journald_reader.next_entry()? {
            let entry_timestamp = entry.get_source_wallclock_time().context("entry is missing time")?.timestamp_us;
            let entry_source = entry.get_field("SYSLOG_IDENTIFIER").unwrap_or("N/A").to_owned();
            let entry_message = entry.get_message().unwrap_or("N/A").to_owned();

            if verbose {
                let datetime = chrono::DateTime::from_timestamp_micros(entry_timestamp).context("invalid time in entry")?.with_timezone(&chrono::Local);
                let time_friendly = format!("{}.{:03}", datetime.format("%Y-%m-%d %H:%M:%S"), datetime.timestamp_subsec_millis());

                outputln!("journald", "new event: {time_friendly} | {entry_source} | {}", entry.get_message().unwrap_or("N/A"));
            }

            temp_cache.push((entry_timestamp, entry_source, entry_message));
        }

        // optimization: don't spawn a runtime for every record, wait till the end
        if !temp_cache.is_empty() {
            tokio_runtime.block_on(async {
                for entry in temp_cache {
                    log_queue::add_entry(identity.clone(), entry.0, json!({
                        "source": entry.1,
                        "message": entry.2
                    }).to_string()).await;
                }
            });
        }
    }
}

pub async fn start(identity: String) -> anyhow::Result<()> {
    let mut running_check = RUNNING_CHECK.lock().await;

    if *running_check {
        return Err(anyhow!("already running, only 1 instance can be running"));
    }

    let tokio_runtime = tokio::runtime::Handle::current();

    // journald crate is not tokio aware
    std::thread::spawn(|| {
        if let Err(e) = worker_thread(tokio_runtime, identity) {
            errorln!("journald", "reader failed with reason: {}; no more journald logs will be read", e.to_string());
        }
    });

    *running_check = true;

    Ok(())
}

pub async fn initialize() {
    let config_clone = config::get_clone().await;

    unsafe {
        VERBOSE = Some(config_clone.verbose);
    }
}
