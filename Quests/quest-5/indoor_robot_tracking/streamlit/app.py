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
st.title("Indoor Robot Tracking (Two Robots)")

# Sidebar: Select refresh interval and historical time range
refresh_interval = st.sidebar.slider("Refresh Interval (seconds)", 1, 10, 2)
time_range_seconds = st.sidebar.slider("Time Range (seconds)", 10, 300, 60)

# Fetch data
df = fetch_positions()

if not df.empty:
    # Ensure 'time' column is in datetime format
    df['time'] = pd.to_datetime(df['time']).dt.tz_convert("UTC")

    # Sidebar: Robot ID selection
    robot_ids = df['id'].unique()
    if len(robot_ids) < 2:
        st.error("Not enough robots in the dataset. Please ensure at least two robots are available.")
    else:
        selected_robot_1 = st.sidebar.selectbox("Select Robot 1 ID", robot_ids, index=0)
        selected_robot_2 = st.sidebar.selectbox("Select Robot 2 ID", robot_ids, index=1)

        # Placeholders for real-time updates and historical tracks
        latest_position_container = st.empty()
        live_position_container = st.empty()
        historical_tracks_1_container = st.empty()
        historical_tracks_2_container = st.empty()

        iteration = 0  # Initialize an iteration counter

        # Main loop for real-time updates
        while True:
            # Fetch the latest positions
            df = fetch_positions()

            if not df.empty:
                df['time'] = pd.to_datetime(df['time']).dt.tz_convert("UTC")

                # Filter data for the selected robots
                filtered_1 = df[df['id'] == selected_robot_1]
                filtered_2 = df[df['id'] == selected_robot_2]

                # Get the latest positions for the selected robots
                latest_position_1 = filtered_1.iloc[-1:]  # Get the last row for robot 1
                latest_position_2 = filtered_2.iloc[-1:]  # Get the last row for robot 2

                # Filter historical data based on the selected time range
                current_time = datetime.now(timezone.utc)
                historical_filtered_1 = filtered_1[
                    filtered_1['time'] >= current_time - timedelta(seconds=time_range_seconds)
                ]
                historical_filtered_2 = filtered_2[
                    filtered_2['time'] >= current_time - timedelta(seconds=time_range_seconds)
                ]

                # Update the latest position table
                with latest_position_container:
                    st.header("Latest Positions")
                    st.dataframe(pd.concat([latest_position_1, latest_position_2]))

                # Update the live position graph with fixed bounds
                with live_position_container:
                    st.header("Live Robot Positions")
                    live_fig = px.scatter(
                        pd.concat([latest_position_1, latest_position_2]),
                        x='x',
                        y='z',
                        color='id',
                        title="Live Positions",
                        labels={'x': 'X Coordinate', 'z': 'Z Coordinate', 'id': 'Robot ID'}
                    )
                    live_fig.update_layout(
                        xaxis=dict(range=[-1000, 1000]),
                        yaxis=dict(range=[-1000, 1000])
                    )
                    st.plotly_chart(live_fig, key=f"live_chart_{iteration}")

                # Update historical scatter plot for Robot 1
                with historical_tracks_1_container:
                    st.header(f"Historical Tracks - Robot {selected_robot_1}")
                    historical_fig_1 = px.scatter(
                        historical_filtered_1,
                        x='x',
                        y='z',
                        color='time',
                        title=f"Historical Tracks (Robot {selected_robot_1}, Last {time_range_seconds} Seconds)",
                        labels={'x': 'X Coordinate', 'z': 'Z Coordinate', 'time': 'Timestamp'},
                        color_continuous_scale="Viridis"
                    )
                    historical_fig_1.update_layout(
                        xaxis=dict(range=[-1000, 1000]),
                        yaxis=dict(range=[-1000, 1000])
                    )
                    st.plotly_chart(historical_fig_1, key=f"historical_chart_1_{iteration}")

                # Update historical scatter plot for Robot 2
                with historical_tracks_2_container:
                    st.header(f"Historical Tracks - Robot {selected_robot_2}")
                    historical_fig_2 = px.scatter(
                        historical_filtered_2,
                        x='x',
                        y='z',
                        color='time',
                        title=f"Historical Tracks (Robot {selected_robot_2}, Last {time_range_seconds} Seconds)",
                        labels={'x': 'X Coordinate', 'z': 'Z Coordinate', 'time': 'Timestamp'},
                        color_continuous_scale="Plasma"
                    )
                    historical_fig_2.update_layout(
                        xaxis=dict(range=[-1000, 1000]),
                        yaxis=dict(range=[-1000, 1000])
                    )
                    st.plotly_chart(historical_fig_2, key=f"historical_chart_2_{iteration}")

                # Increment iteration counter for unique keys
                iteration += 1

            # Wait before updating again
            time.sleep(refresh_interval)
else:
    st.warning("No data found in the database.")
