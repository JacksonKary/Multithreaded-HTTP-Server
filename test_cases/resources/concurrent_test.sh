#! /bin/bash

target_files=(
    "quote.txt"
    "headers.html"
    "index.html"
    "courses.txt"
    "mt2_practice.pdf"
    "gatsby.txt"
    "africa.jpg"
    "ocelot.jpg"
    "hard_drive.png"
    "Lec01.pdf"
)

rm -rf downloaded_files
mkdir -p downloaded_files
echo "Starting HTTP Server"
LD_PRELOAD=./concurrent_open.so ./http_server server_files $PORT &
http_server_pid=$!

curl_pids=( )
for target_file in ${target_files[@]}
do
    echo "Starting request for file $target_file"
    curl -s -S http://localhost:$PORT/$target_file > downloaded_files/$target_file &
    curl_pids+=($!)
done

echo "Waiting for HTTP responses"
# Wait for all 'curl' commands to complete
for curl_pid in ${curl_pids[@]}
do
    wait $curl_pid
done
echo "All HTTP responses received"

# Send SIGINT to http_server process and wait for it to complete
# This verifies that the server's shutdown logic works properly
echo "Sending SIGINT to trigger server shutdown"
kill -INT $http_server_pid
wait $http_server_pid
echo "Server has terminated"

# Finally, verify contents of retrieved files
for target_file in ${target_files[@]}
do
    diff -q server_files/$target_file downloaded_files/$target_file
done
