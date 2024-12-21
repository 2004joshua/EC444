import streamlit as st
import pandas as pd
import requests
import plotly.express as px
import time
from datetime import datetime, timedelta, timezone

# Function to fetch positional data from Node.js server
def fetch_positions():
    try:
        response = requests.get("http://localhost:3000/positions")
        response.raise_for_status()
        return pd.DataFrame(response.json())
    except requests.exceptions.RequestException as e:
        st.error(f"Error fetching data: {e}")
        return pd.DataFrame()

# Streamlit app
st.title("Indoor Robot Tracking")

# Sidebar: Select refresh interval and robot ID
refresh_interval = st.sidebar.slider("Refresh Interval (seconds)", 1, 10, 2)

# Sidebar: Select historical time range in seconds
time_range_seconds = st.sidebar.slider("Time Range (seconds)", 10, 300, 60)

# Fetch data once for sidebar filters
df = fetch_positions()

if not df.empty:
    # Ensure 'time' column is in datetime format
    df['time'] = pd.to_datetime(df['time']).dt.tz_convert("UTC")

    # Sidebar: Robot ID selection
    robot_ids = df['id'].unique()
    selected_robot_id = st.sidebar.selectbox("Select Robot ID", robot_ids, index=0)

    # Placeholders for real-time updates and historical tracks
    latest_position_container = st.empty()
    live_position_container = st.empty()
    historical_tracks_container = st.empty()

    iteration = 0  # Initialize an iteration counter

    # Main loop for real-time updates
    while True:
        # Fetch the latest positions
        df = fetch_positions()

        if not df.empty:
            df['time'] = pd.to_datetime(df['time']).dt.tz_convert("UTC")

            # Filter data for the selected robot
            filtered = df[df['id'] == selected_robot_id]

            # Get the latest position for the selected robot
            latest_position = filtered.iloc[-1:]  # Get the last row

            # Filter historical data based on the selected time range
            current_time = datetime.now(timezone.utc)  # Make current_time timezone-aware
            historical_filtered = filtered[
                filtered['time'] >= current_time - timedelta(seconds=time_range_seconds)
            ]

            # Update the latest position table
            with latest_position_container:
                st.header("Latest Position")
                st.dataframe(latest_position)

            # Update the live position graph with fixed bounds
            with live_position_container:
                st.header("Live Robot Position")
                live_fig = px.scatter(
                    latest_position,
                    x='x',
                    y='z',
                    title="Live Position",
                    labels={'x': 'X Coordinate', 'z': 'Z Coordinate'}
                )
                # Set fixed axis ranges
                live_fig.update_layout(
                    xaxis=dict(range=[-1000, 1000]),
                    yaxis=dict(range=[-1000, 1000])
                )
                st.plotly_chart(live_fig, key=f"live_chart_{iteration}")

            # Update historical scatter plot with fixed bounds
            with historical_tracks_container:
                st.header("Historical Tracking")
                historical_fig = px.scatter(
                    historical_filtered,
                    x='x',
                    y='z',
                    color='time',
                    title="Historical Tracks (Last {} Seconds)".format(time_range_seconds),
                    labels={'x': 'X Coordinate', 'z': 'Z Coordinate', 'time': 'Timestamp'},
                    color_continuous_scale="Viridis"
                )
                # Set fixed axis ranges
                historical_fig.update_layout(
                    xaxis=dict(range=[-1000, 1000]),
                    yaxis=dict(range=[-1000, 1000])
                )
                st.plotly_chart(historical_fig, key=f"historical_chart_{iteration}")

            # Increment iteration counter for unique keys
            iteration += 1

        # Wait before updating again
        time.sleep(refresh_interval)
else:
    st.warning("No data found in the database.")
