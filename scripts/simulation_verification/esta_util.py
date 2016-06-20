import pandas as pd

def load_histogram_csv(filename):
    print "Loading " + filename + "..."
    return pd.read_csv(filename).sort_values(by="delay")


def load_exhaustive_csv(filename):
    print "Loading " + filename + "..."
    raw_data = pd.read_csv(filename)

    return transitions_to_histogram(raw_data)

def transitions_to_histogram(raw_data):
    #Counts of how often all delay values occur
    raw_counts = raw_data['delay'].value_counts(sort=False)
    
    #Normalize by total combinations (i.e. number of rows)
    #to get probability
    normed_counts = raw_counts / raw_data.shape[0]

    df = pd.DataFrame({"delay": normed_counts.index, "probability": normed_counts.values})

    #Is there a zero probability entry?
    if not df[df['delay'] == 0.].shape[0]:
        #If not, add a zero delay @ probability zero if none is recorded
        #this ensures matplotlib draws the CDF correctly
        zero_delay_df = pd.DataFrame({"delay": [0.], "probability": [0.]})
        
        df = df.append(zero_delay_df)

    return df.sort_values(by="delay")
