import streamlit as st
import pandas as pd
import json

# Path to the exported JSON file
data_file = "../data/exported_data.json"

st.title("TingoDB Data Visualization")
st.subheader("Explore and visualize sensor data")

try:
    # Load the JSON data
    with open(data_file, 'r') as f:
        data = json.load(f)

    if data:
        # Convert to a Pandas DataFrame
        df = pd.DataFrame(data)

        # Display raw data
        st.write("### Raw Data")
        st.dataframe(df)

        # Plot temperature vs. time
        if "time" in df.columns and "temperature" in df.columns:
            st.write("### Temperature Over Time")
            st.line_chart(df[["time", "temperature"]].set_index("time"))

        # Add filter options for Sensor ID
        sensor_id = st.selectbox("Select Sensor ID:", sorted(df["sensor_id"].unique()))
        filtered_data = df[df["sensor_id"] == sensor_id]

        st.write(f"### Filtered Data for Sensor ID {sensor_id}")
        st.dataframe(filtered_data)

        # Plot filtered data
        if not filtered_data.empty:
            st.write(f"### Temperature Over Time for Sensor ID {sensor_id}")
            st.line_chart(filtered_data[["time", "temperature"]].set_index("time"))
    else:
        st.warning("No data found in the JSON file.")
except FileNotFoundError:
    st.error(f"Data file not found: {data_file}")
except json.JSONDecodeError as e:
    st.error(f"Error decoding JSON: {e}")
